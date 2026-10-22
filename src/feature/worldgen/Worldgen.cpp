#include "feature/worldgen/Worldgen.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <mutex>
#include <set>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/string/HashedString.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/ChunkPos.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/LevelSeed64.h"
#include "mc/world/level/biome/Biome.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockSupportType.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/chunk/LevelChunk.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/material/Material.h"

#include "game/Blocks.h"
#include "game/Ids.h"
#include "game/Waystones.h"
#include "game/WorldDb.h"
#include "mod/Config.h"
#include "mod/Log.h"
#include "mod/Module.h"

BS_REGISTER_CONFIG("worldGen", bs::feature::worldgen::WorldGenConfig);

namespace bs::feature::worldgen {

namespace cw = core::worldgen;

namespace {

constexpr std::string_view kDbKey           = "bs:worldgen";
constexpr std::size_t      kMaxQueued       = 16384; ///< per queue
constexpr int              kLoadedPerTick   = 256;   ///< load reports handled per tick (a hash each)
constexpr int              kFreshPerTick    = 8;     ///< village scans per tick (a few thousand block reads each)
constexpr int              kSweepInterval   = 100;   ///< ticks between player-area sweeps
constexpr int              kRetryInterval   = 20;
constexpr int              kMaxRetries      = 60;
constexpr int              kVillageSpacing  = 64; ///< bells closer than this belong to one village
constexpr int              kNeighbourRadius = 24; ///< blocks: the chunks around a spot must be loaded (decorated)
constexpr int              kWildSearch      = 3;  ///< columns tried around a blocked wild spot

struct ChunkReport {
    int  dim = 0, x = 0, z = 0;
    bool fresh   = false;
    int  retries = 0;
};

struct PendingVillage {
    int      dim = 0;
    BlockPos center;
    int      retries = 0;
};

// Hooks may run on chunk worker threads: they only push reports under this mutex.
std::mutex              gQueueMutex;
std::deque<ChunkReport> gLoaded; ///< every chunk load (wild cells)
std::deque<ChunkReport> gFresh;  ///< freshly generated chunks (village scan)
// Server thread only.
std::vector<ChunkReport>    gRetry;
std::vector<PendingVillage> gPendingVillages;

struct State {
    bool                         loaded = false;
    bool                         dirty  = false;
    std::set<std::string>        cells;
    std::set<std::string>        villages;
    std::vector<cw::VillageKey>  villageList; ///< parsed `villages` (distance checks)
} gState;

std::string            gTestBlock;
std::set<std::string>  gMissingLogged;
ll::event::ListenerPtr gTickListener;
int                    gTick = 0;

void addVillage(State& st, std::string const& key) {
    if (!st.villages.insert(key).second) return;
    if (auto v = cw::parseVillageKey(key)) st.villageList.push_back(*v);
}

State& state() {
    if (!gState.loaded && game::db::available()) {
        // Marks made before the DB was available are kept (merged with the stored ones).
        gState.loaded = true;
        try {
            auto j = game::db::load(std::string(kDbKey));
            if (j && j->is_object()) {
                if (auto c = j->find("cells"); c != j->end() && c->is_array()) {
                    for (auto const& e : *c) {
                        if (e.is_string()) gState.cells.insert(e.get<std::string>());
                    }
                }
                if (auto v = j->find("villages"); v != j->end() && v->is_array()) {
                    for (auto const& e : *v) {
                        if (e.is_string()) addVillage(gState, e.get<std::string>());
                    }
                }
            }
        } catch (std::exception const& e) {
            log().warn("worldgen: could not read {}: {}", kDbKey, e.what());
        }
    }
    return gState;
}

template <class T>
void push(std::deque<T>& q, T r) {
    if (q.size() < kMaxQueued) q.push_back(r);
}

BlockSource* regionOf(int dim) {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    auto dimension = level->getDimension(DimensionType(dim)).lock();
    if (!dimension) return nullptr;
    return &dimension->getBlockSourceFromMainChunkSource();
}

bool hasCeiling(int dim) {
    auto level = ll::service::getLevel();
    if (!level) return false;
    auto dimension = level->getDimension(DimensionType(dim)).lock();
    return dimension && dimension->mHasCeiling;
}

bool loadedAt(BlockSource& region, int x, int z) { return region.hasChunksAt(BlockPos{x, 0, z}, 0, false); }
/// The chunk at (x, z) and its neighbours are loaded, i.e. decorated: a waystone placed now is
/// not overwritten by a neighbour's trees or village pieces.
bool areaReady(BlockSource& region, int x, int z) { return region.hasChunksAt(BlockPos{x, 0, z}, kNeighbourRadius, false); }

/// Waystones' OCEAN_FLOOR predicate: blocks motion and is not a fluid.
bool solid(Block const& b) {
    if (b.isAir()) return false;
    auto const& m = b.getMaterial();
    return m.mBlocksMotion && !m.mLiquid;
}

/// Solid terrain: Waystones uses the world-generation heightmap, taken before trees and huge
/// mushrooms are decorated, so their blocks don't count as ground.
bool terrain(Block const& b) {
    if (!solid(b)) return false;
    auto const& name = b.getTypeName();
    static constexpr std::array<std::string_view, 9> kFeatures{
        "leaves", "_log", "_wood", "_stem", "mushroom_block", "mangrove_roots", "bamboo", "bee_nest", "_hyphae",
    };
    return std::none_of(kFeatures.begin(), kFeatures.end(), [&name](std::string_view f) {
        return name.find(f) != std::string::npos;
    });
}

std::string biomeAt(BlockSource& region, BlockPos const& pos) { return region.getBiome(pos).mHash->getString(); }

std::string posText(BlockPos const& p) { return std::to_string(p.x) + " " + std::to_string(p.y) + " " + std::to_string(p.z); }

/// The block to place for `style`: bedstones:<style>, or the debug test block when it is missing.
std::string blockFor(std::string_view style, std::string_view blockOverride) {
    std::string id = ids::id(style);
    if (game::blockWith(id)) return id;
    std::string fallback(blockOverride.empty() ? std::string_view(gTestBlock) : blockOverride);
    if (!fallback.empty() && game::blockWith(fallback)) return fallback;
    if (gMissingLogged.insert(id).second) log().debug("worldgen: block {} is not registered, waystone generation skipped", id);
    return {};
}

/// Village spot candidates (dx, dz), 4..12 blocks from the centre, nearest first.
std::vector<std::pair<int, int>> const& villageOffsets() {
    static auto const offsets = [] {
        std::vector<std::pair<int, int>> v;
        for (int dx = -12; dx <= 12; ++dx) {
            for (int dz = -12; dz <= 12; ++dz) {
                int d2 = dx * dx + dz * dz;
                if (d2 >= 16 && d2 <= 144) v.emplace_back(dx, dz);
            }
        }
        std::stable_sort(v.begin(), v.end(), [](auto const& a, auto const& b) {
            return a.first * a.first + a.second * a.second < b.first * b.first + b.second * b.second;
        });
        return v;
    }();
    return offsets;
}

} // namespace

// ---- hooks --------------------------------------------------------------------------------------

LL_TYPE_INSTANCE_HOOK(
    WorldgenChunkLoadedHook,
    ll::memory::HookPriority::Normal,
    Dimension,
    &Dimension::$onChunkLoaded,
    void,
    ::ChunkSource& source,
    ::LevelChunk&  lc
) {
    origin(source, lc);
    // `this` is the LevelListener base subobject here: read everything from the chunk.
    if (lc.mLevel.isClientSide()) return;
    auto const&     pos = *lc.mPosition;
    std::lock_guard lock{gQueueMutex};
    push(gLoaded, ChunkReport{static_cast<int>(lc.mDimension.getDimensionId()), pos.x, pos.z, false, 0});
}

LL_TYPE_INSTANCE_HOOK(
    WorldgenPostProcessHook,
    ll::memory::HookPriority::Normal,
    LevelChunk,
    &LevelChunk::finalizePostProcessing,
    void
) {
    origin();
    if (mLevel.isClientSide()) return;
    auto const&     pos = *mPosition;
    std::lock_guard lock{gQueueMutex};
    push(gFresh, ChunkReport{static_cast<int>(mDimension.getDimensionId()), pos.x, pos.z, true, 0});
}

// ---- config ---------------------------------------------------------------------------------------

std::string_view toString(PlaceResult r) {
    switch (r) {
    case PlaceResult::Placed:
        return "placed";
    case PlaceResult::NoSpace:
        return "no space";
    case PlaceResult::Occupied:
        return "occupied";
    case PlaceResult::BlockMissing:
        return "block missing";
    case PlaceResult::NotLoaded:
        return "not loaded";
    case PlaceResult::Disabled:
        return "disabled";
    }
    return "?";
}

cw::WildStyle wildStyle() {
    return cw::wildStyleFromString(config::get<WorldGenConfig>().wildWaystoneStyle).value_or(cw::WildStyle::Biome);
}

cw::VillageMode villageMode() {
    return cw::villageModeFromString(config::get<WorldGenConfig>().spawnInVillages).value_or(cw::VillageMode::Regular);
}

bool wildEnabledIn(int dim) {
    auto const& c = config::get<WorldGenConfig>();
    return c.chunksBetweenWildWaystones > 0
        && cw::dimensionAllowed(dim, c.wildWaystonesDimensionAllowList, c.wildWaystonesDimensionDenyList);
}

std::uint64_t worldSeed() {
    auto level = ll::service::getLevel();
    return level ? level->getLevelSeed64().mValue : 0;
}

// ---- placement --------------------------------------------------------------------------------------

std::optional<int> wildSurfaceY(BlockSource& region, int x, int z, bool ceiling) {
    int minY = region.getMinHeight();
    int maxY = region.getMaxHeight();
    if (ceiling) {
        for (int y = maxY - 2; y > minY; --y) {
            if (region.getBlock(BlockPos{x, y, z}).isAir() && region.getBlock(BlockPos{x, y + 1, z}).isAir()
                && terrain(region.getBlock(BlockPos{x, y - 1, z}))) {
                return y;
            }
        }
        return std::nullopt;
    }
    int top = std::clamp(int(region.getHeightmapPos(BlockPos{x, 0, z}).y), minY + 1, maxY - 1);
    for (int y = top; y > minY; --y) {
        if (terrain(region.getBlock(BlockPos{x, y - 1, z}))) return y;
    }
    return std::nullopt;
}

PlaceResult placeWaystone(
    BlockSource&     region,
    BlockPos const&  pos,
    std::string_view style,
    int              facing,
    core::Origin     origin,
    std::string*     uidOut,
    std::string_view blockOverride
) {
    if (!loadedAt(region, pos.x, pos.z)) return PlaceResult::NotLoaded;
    auto& ws = game::Waystones::get();
    if (!ws.available()) return PlaceResult::NotLoaded;
    auto gpos = game::globalPos(region, pos);
    if (ws.findAt(gpos)) return PlaceResult::Occupied;

    auto blockId = blockFor(style, blockOverride);
    if (blockId.empty()) return PlaceResult::BlockMissing;
    if (!game::placeTwoTall(region, pos, blockId, facing)) return PlaceResult::NoSpace;

    core::WaystoneRecord r;
    r.type       = std::string(core::types::kWaystone);
    r.pos        = gpos;
    r.style      = std::string(style);
    r.facing     = facing & 3;
    r.origin     = origin;
    r.visibility = core::Visibility::Activation;
    auto* rec    = ws.create(std::move(r));
    if (!rec) {
        game::clearBlock(region, BlockPos{pos.x, pos.y + 1, pos.z});
        game::clearBlock(region, pos);
        return PlaceResult::NotLoaded;
    }
    if (uidOut) *uidOut = rec->uid;
    return PlaceResult::Placed;
}

PlaceResult generateWild(int dim, cw::WildSpot const& spot) {
    if (!wildEnabledIn(dim)) return PlaceResult::Disabled;
    int  n   = config::get<WorldGenConfig>().chunksBetweenWildWaystones;
    auto key = cw::cellKey(dim, n, spot.cell);
    if (isCellDone(key)) return PlaceResult::Occupied;
    auto* region = regionOf(dim);
    if (!region || !areaReady(*region, spot.blockX, spot.blockZ)) return PlaceResult::NotLoaded;

    bool ceiling = hasCeiling(dim);
    // Style from the biome at the surface; skip the scans while its block doesn't exist.
    int  probeY  = std::clamp(int(region->getHeightmapPos(BlockPos{spot.blockX, 0, spot.blockZ}).y), int(region->getMinHeight()), int(region->getMaxHeight()) - 1);
    if (ceiling) probeY = (region->getMinHeight() + region->getMaxHeight()) / 2;
    auto style = std::string(cw::resolveStyle(wildStyle(), biomeAt(*region, BlockPos{spot.blockX, probeY, spot.blockZ}), dim));
    if (blockFor(style, {}).empty()) return PlaceResult::BlockMissing;

    // The spot's column first, then the nearest columns around it (tree trunks, boulders, ...).
    PlaceResult last = PlaceResult::NoSpace;
    for (int r = 0; r <= kWildSearch; ++r) {
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (std::max(std::abs(dx), std::abs(dz)) != r) continue;
                int  x = spot.blockX + dx, z = spot.blockZ + dz;
                auto y = wildSurfaceY(*region, x, z, ceiling);
                if (!y) continue;
                BlockPos pos{x, *y, z};
                last = placeWaystone(*region, pos, style, spot.facing, core::Origin::Wilderness);
                if (last == PlaceResult::Placed) {
                    markCellDone(key);
                    log().info("wild waystone placed at {} in {} (style {})", posText(pos), cw::dimensionName(dim), style);
                    return last;
                }
                if (last == PlaceResult::BlockMissing || last == PlaceResult::NotLoaded) return last; // retried later
            }
        }
    }
    markCellDone(key);
    log().info("wild waystone skipped at {} ~ {} in {}: {}", spot.blockX, spot.blockZ, cw::dimensionName(dim), toString(last));
    return PlaceResult::NoSpace;
}

std::optional<BlockPos> findBell(BlockSource& region, int chunkX, int chunkZ) {
    if (!loadedAt(region, chunkX * 16 + 8, chunkZ * 16 + 8)) return std::nullopt;
    int minY = region.getMinHeight();
    for (int dx = 0; dx < 16; ++dx) {
        for (int dz = 0; dz < 16; ++dz) {
            int x   = chunkX * 16 + dx;
            int z   = chunkZ * 16 + dz;
            int top = region.getHeightmapPos(BlockPos{x, 0, z}).y;
            for (int y = top; y >= std::max(top - 12, minY); --y) {
                BlockPos p{x, y, z};
                if (region.getBlock(p).getTypeName() == "minecraft:bell") return p;
            }
        }
    }
    return std::nullopt;
}

std::optional<BlockPos> villageSpot(BlockSource& region, BlockPos const& center) {
    for (auto [dx, dz] : villageOffsets()) {
        int x = center.x + dx, z = center.z + dz;
        if (!loadedAt(region, x, z)) continue;
        int top = region.getHeightmapPos(BlockPos{x, 0, z}).y; // first air above the column
        for (int y = center.y + 3; y >= center.y - 3; --y) {
            BlockPos    p{x, y, z};
            auto const& below = region.getBlock(BlockPos{x, y - 1, z});
            if (!terrain(below) || !below.canProvideSupport(1, BlockSupportType::Center)) continue;
            if (!region.getBlock(p).isAir() || !region.getBlock(BlockPos{x, y + 1, z}).isAir()) continue;
            if (top > y + 2) break; // roofed (inside a house): try the next column
            return p;
        }
    }
    return std::nullopt;
}

PlaceResult generateVillage(int dim, BlockPos const& center, std::string* uidOut) {
    auto mode = villageMode();
    if (mode == cw::VillageMode::Disabled) return PlaceResult::Disabled;
    auto& st = state();
    // One waystone per village: villages with more than one bell are recognised by distance.
    for (auto const& v : st.villageList) {
        if (v.dim == dim && std::hypot(double(v.x - center.x), double(v.z - center.z)) < kVillageSpacing) {
            return PlaceResult::Occupied;
        }
    }
    auto* region = regionOf(dim);
    if (!region || !areaReady(*region, center.x, center.z)) return PlaceResult::NotLoaded;
    auto key  = cw::villageKey(dim, center.x, center.z);
    auto done = [&st, &key] {
        addVillage(st, key);
        st.dirty = true;
    };
    if (!cw::villageGetsWaystone(mode, worldSeed(), dim, center.x, center.z)) {
        done();
        log().info("village at {} in {}: no waystone (chance)", posText(center), cw::dimensionName(dim));
        return PlaceResult::Disabled;
    }
    auto style = std::string(cw::villageStyle(biomeAt(*region, center)));
    if (blockFor(style, {}).empty()) return PlaceResult::BlockMissing;
    auto spot = villageSpot(*region, center);
    if (!spot) {
        done();
        log().info("village waystone skipped at {} in {}: no space", posText(center), cw::dimensionName(dim));
        return PlaceResult::NoSpace;
    }
    // Face the village centre.
    int dx = center.x - spot->x, dz = center.z - spot->z;
    int facing = std::abs(dx) > std::abs(dz) ? (dx > 0 ? 3 : 1) : (dz > 0 ? 0 : 2);
    auto res   = placeWaystone(*region, *spot, style, facing, core::Origin::Village, uidOut);
    if (res == PlaceResult::Placed || res == PlaceResult::NoSpace || res == PlaceResult::Occupied) done();
    if (res == PlaceResult::Placed) {
        log().info("village waystone placed at {} in {} (style {}, village {})", posText(*spot), cw::dimensionName(dim), style, posText(center));
    }
    return res;
}

PlaceResult processChunk(int dim, int chunkX, int chunkZ, bool fresh) {
    auto wild = PlaceResult::Disabled;
    if (wildEnabledIn(dim)) {
        int n = config::get<WorldGenConfig>().chunksBetweenWildWaystones;
        if (auto spot = cw::spotInChunk(worldSeed(), chunkX, chunkZ, n)) wild = generateWild(dim, *spot);
    }
    if (fresh && villageMode() != cw::VillageMode::Disabled) {
        if (auto* region = regionOf(dim)) {
            if (auto bell = findBell(*region, chunkX, chunkZ)) {
                log().info("village detected at {} in {}", posText(*bell), cw::dimensionName(dim));
                // Villages are only reported once (fresh chunks): keep retrying while the
                // surrounding chunks load.
                if (generateVillage(dim, *bell) == PlaceResult::NotLoaded) gPendingVillages.push_back({dim, *bell, 0});
            }
        }
    }
    return wild;
}

// ---- state ---------------------------------------------------------------------------------------------

bool isCellDone(std::string const& key) { return state().cells.contains(key); }
void markCellDone(std::string const& key) {
    if (state().cells.insert(key).second) gState.dirty = true;
}
void forgetCell(std::string const& key) {
    if (state().cells.erase(key)) gState.dirty = true;
}
bool isVillageDone(std::string const& key) { return state().villages.contains(key); }
void forgetVillage(std::string const& key) {
    auto& st = state();
    if (!st.villages.erase(key)) return;
    st.villageList.clear();
    for (auto const& k : st.villages) {
        if (auto v = cw::parseVillageKey(k)) st.villageList.push_back(*v);
    }
    st.dirty = true;
}

void saveState() {
    if (!gState.loaded || !game::db::available()) return;
    nlohmann::json j{{"version", 1}, {"cells", gState.cells}, {"villages", gState.villages}};
    game::db::save(std::string(kDbKey), j);
    gState.dirty = false;
}

void reloadState() { gState = State{}; }

void setTestBlock(std::string id) {
    gTestBlock = std::move(id);
    gMissingLogged.clear();
}
std::string const& testBlock() { return gTestBlock; }

namespace {

bool chunkLoaded(ChunkReport const& r) {
    auto* region = regionOf(r.dim);
    return region && loadedAt(*region, r.x * 16 + 8, r.z * 16 + 8);
}

void handle(ChunkReport r) {
    if (!chunkLoaded(r)) {
        if (++r.retries <= kMaxRetries) gRetry.push_back(r);
        return;
    }
    // The wild spot waits for its neighbour chunks: retry the (cheap) load report.
    if (processChunk(r.dim, r.x, r.z, r.fresh) == PlaceResult::NotLoaded && ++r.retries <= kMaxRetries) {
        r.fresh = false;
        gRetry.push_back(r);
    }
}

/// Wild cells around every player whose waystone area is loaded (covers chunks that were loaded
/// before the mod was enabled, spots that waited for their neighbours, and config changes).
void sweepPlayers(Level& level) {
    int n = config::get<WorldGenConfig>().chunksBetweenWildWaystones;
    if (n <= 0) return;
    auto seed = worldSeed();
    std::vector<std::pair<int, cw::Cell>> cells;
    level.forEachPlayer([&](Player& p) {
        if (p.isClientSide()) return true;
        int  dim  = static_cast<int>(p.getDimensionId());
        auto pos  = p.getPosition();
        auto home = cw::cellOf(
            cw::floorDiv(static_cast<int>(std::floor(pos.x)), 16),
            cw::floorDiv(static_cast<int>(std::floor(pos.z)), 16),
            n
        );
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) cells.emplace_back(dim, cw::Cell{home.x + dx, home.z + dz});
        }
        return true;
    });
    for (auto const& [dim, cell] : cells) {
        if (!wildEnabledIn(dim) || isCellDone(cw::cellKey(dim, n, cell))) continue;
        auto  spot   = cw::wildSpot(seed, cell, n);
        auto* region = regionOf(dim);
        if (region && areaReady(*region, spot.blockX, spot.blockZ)) generateWild(dim, spot);
    }
}

void retryVillages() {
    auto pending = std::move(gPendingVillages);
    gPendingVillages.clear();
    for (auto v : pending) {
        if (generateVillage(v.dim, v.center) == PlaceResult::NotLoaded && ++v.retries <= kMaxRetries) {
            gPendingVillages.push_back(v);
        }
    }
}

void onTick(Level& level) {
    ++gTick;
    if (!game::Waystones::get().available()) return;
    std::vector<ChunkReport> batch;
    {
        std::lock_guard lock{gQueueMutex};
        for (int i = 0; i < kLoadedPerTick && !gLoaded.empty(); ++i) {
            batch.push_back(gLoaded.front());
            gLoaded.pop_front();
        }
        for (int i = 0; i < kFreshPerTick && !gFresh.empty(); ++i) {
            batch.push_back(gFresh.front());
            gFresh.pop_front();
        }
    }
    for (auto const& r : batch) handle(r);
    if (gTick % kRetryInterval == 0) {
        auto retry = std::move(gRetry);
        gRetry.clear();
        for (auto const& r : retry) handle(r);
        if (!gPendingVillages.empty()) retryVillages();
    }
    if (gTick % kSweepInterval == 0) sweepPlayers(level);
}

} // namespace

class WorldgenModule {
public:
    void install() {
        static bool registered = false;
        if (!registered) {
            registered = true;
            game::db::onWorldSave([] {
                if (gState.dirty) saveState();
            });
            game::db::onWorldClosed([] {
                reloadState();
                gRetry.clear();
                gPendingVillages.clear();
                std::lock_guard lock{gQueueMutex};
                gLoaded.clear();
                gFresh.clear();
            });
        }
        gTickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>(
            [](ll::event::ServerLevelTickEvent& ev) { onTick(ev.level()); }
        );
        WorldgenChunkLoadedHook::hook();
        WorldgenPostProcessHook::hook();
    }
    void uninstall() {
        WorldgenPostProcessHook::unhook();
        WorldgenChunkLoadedHook::unhook();
        if (gTickListener) ll::event::EventBus::getInstance().removeListener(gTickListener);
        gTickListener.reset();
        if (gState.dirty) saveState();
    }
};

BS_REGISTER_MODULE(WorldgenModule);

} // namespace bs::feature::worldgen
