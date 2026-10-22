// Selftests "worldgen.*" (test area dx 1500..1599).

#include <algorithm>

#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"

#include "feature/worldgen/Worldgen.h"
#include "game/Blocks.h"
#include "game/Ids.h"
#include "game/Waystones.h"
#include "mod/Config.h"
#include "selftest/SelfTest.h"

namespace {

using bs::selftest::Context;
namespace wg = bs::feature::worldgen;
namespace cw = bs::core::worldgen;

BlockSource* overworld() {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    auto dim = level->getDimension(DimensionType(0)).lock();
    return dim ? &dim->getBlockSourceFromMainChunkSource() : nullptr;
}

/// Moves the test player to `pos` and waits until the area around it is loaded.
ll::coro::CoroTask<bool> loadArea(Context& ctx, bs::core::GlobalPos pos) {
    auto& p = ctx.player();
    float y = float(std::clamp(pos.y, -60, 300)) + 2.f;
    p.teleport(Vec3{float(pos.x) + 0.5f, y, float(pos.z) + 0.5f}, DimensionType(0), Vec2{0.f, 0.f});
    for (int i = 0; i < 200; ++i) {
        co_await ctx.ticks(2);
        auto* region = overworld();
        if (region && region->hasChunksAt(BlockPos{pos.x, 0, pos.z}, 32, false)) co_return true;
    }
    co_return false;
}

/// Builds an open stone platform (radius `r`) whose floor is above the local terrain. Returns the
/// y of the first air layer.
int buildPlatform(Context& ctx, BlockSource& region, bs::core::GlobalPos center, int r) {
    int top = region.getMinHeight(); // ignore center.y: BDS may report an unset spawn height (32767)
    for (int dx = -r; dx <= r; ++dx) {
        for (int dz = -r; dz <= r; ++dz) {
            top = std::max(top, int(region.getHeightmapPos(BlockPos{center.x + dx, 0, center.z + dz}).y));
        }
    }
    top = std::min(top + 1, int(region.getMaxHeight()) - 4);
    for (int dx = -r; dx <= r; ++dx) {
        for (int dz = -r; dz <= r; ++dz) {
            int x = center.x + dx, z = center.z + dz;
            ctx.setBlock({0, x, top - 1, z}, "minecraft:stone");
        }
    }
    return top;
}

/// The block to place when the waystone blocks unit is absent.
std::string standIn() { return bs::game::blockWith("bedstones:waystone") ? "" : "minecraft:stone_bricks"; }

} // namespace

BS_REGISTER_SELFTEST("worldgen.grid", [](Context& ctx) {
    auto seed = wg::worldSeed();
    int  n    = 25;
    for (int gx = -2; gx <= 2; ++gx) {
        for (int gz = -2; gz <= 2; ++gz) {
            auto a = cw::wildSpot(seed, {gx, gz}, n);
            auto b = cw::wildSpot(seed, {gx, gz}, n);
            ctx.check(a.blockX == b.blockX && a.blockZ == b.blockZ && a.facing == b.facing, "spot is deterministic");
            ctx.check(cw::cellOf(a.chunkX, a.chunkZ, n) == (cw::Cell{gx, gz}), "spot lies in its cell");
            ctx.check(a.chunkX - gx * n <= cw::maxOffset(n) && a.chunkZ - gz * n <= cw::maxOffset(n), "offset <= ceil(n/2)");
            ctx.check(cw::spotInChunk(seed, a.chunkX, a.chunkZ, n).has_value(), "the spot chunk is recognised");
        }
    }
    // Exactly one chunk per cell (small grid for speed).
    int small = 4;
    for (int gx = -1; gx <= 1; ++gx) {
        int hits = 0;
        for (int x = gx * small; x < (gx + 1) * small; ++x) {
            for (int z = 0; z < small; ++z) hits += cw::spotInChunk(seed, x, z, small) ? 1 : 0;
        }
        ctx.check(hits == 1, "exactly one waystone chunk per cell");
    }
});

BS_REGISTER_SELFTEST("worldgen.biome_style", [](Context& ctx) {
    ctx.check(cw::styleForBiome("desert", 0) == "sandy_waystone", "desert -> sandy");
    ctx.check(cw::styleForBiome("swampland", 0) == "mossy_waystone", "swamp -> mossy");
    ctx.check(cw::styleForBiome("mangrove_swamp", 0) == "mossy_waystone", "mangrove -> mossy");
    ctx.check(cw::styleForBiome("mushroom_island", 0) == "mossy_waystone", "mushroom -> mossy");
    ctx.check(cw::styleForBiome("jungle", 0) == "mossy_waystone", "jungle -> mossy");
    ctx.check(cw::styleForBiome("hell", 1) == "blackstone_waystone", "nether -> blackstone");
    ctx.check(cw::styleForBiome("warped_forest", 1) == "blackstone_waystone", "warped forest -> blackstone");
    ctx.check(cw::styleForBiome("the_end", 2) == "end_stone_waystone", "end -> end stone");
    ctx.check(cw::styleForBiome("deep_dark", 0) == "deepslate_waystone", "deep dark -> deepslate");
    ctx.check(cw::styleForBiome("plains", 0) == "waystone", "plains -> default");
    auto& cfg = bs::config::mut<wg::WorldGenConfig>();
    auto  old = cfg.wildWaystoneStyle;
    cfg.wildWaystoneStyle = "PRISMARINE";
    ctx.check(wg::wildStyle() == cw::WildStyle::Prismarine, "fixed style from the config");
    cfg.wildWaystoneStyle = old;
    ctx.check(cw::resolveStyle(cw::WildStyle::Biome, "desert", 0) == "sandy_waystone", "BIOME resolves by biome");
});

BS_REGISTER_SELFTEST("worldgen.place", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto center = ctx.testPos(1500);
    ctx.require(co_await loadArea(ctx, center), "test area loaded");
    auto* region = overworld();
    ctx.require(region != nullptr, "overworld region");
    int  y = buildPlatform(ctx, *region, center, 1);
    BlockPos pos{center.x, y, center.z};
    ctx.cleanupBlock({0, pos.x, pos.y, pos.z});
    ctx.cleanupBlock({0, pos.x, pos.y + 1, pos.z});
    auto override = standIn();
    if (!override.empty()) ctx.info("bedstones:waystone is not registered: placing " + override + " instead");

    std::string uid;
    auto res = wg::placeWaystone(*region, pos, "waystone", 1, bs::core::Origin::Wilderness, &uid, override);
    ctx.require(res == wg::PlaceResult::Placed, "placed (" + std::string(wg::toString(res)) + ")");
    ctx.cleanupWaystone(uid);
    auto* rec = bs::game::Waystones::get().find(uid);
    ctx.require(rec != nullptr, "record created");
    ctx.check(rec->origin == bs::core::Origin::Wilderness, "origin WILDERNESS");
    ctx.check(rec->name.empty() && !rec->hasOwner(), "no name, no owner");
    ctx.check(rec->visibility == bs::core::Visibility::Activation, "visibility ACTIVATION");
    ctx.check(rec->style == "waystone" && rec->facing == 1 && rec->isWaystone(), "style / facing / type");
    ctx.check(rec->pos == bs::core::GlobalPos{0, pos.x, pos.y, pos.z}, "record at the lower block");
    std::string expect = override.empty() ? "bedstones:waystone" : override;
    ctx.check(bs::game::blockIdAt(*region, pos) == expect, "lower block placed");
    ctx.check(bs::game::blockIdAt(*region, BlockPos{pos.x, pos.y + 1, pos.z}) == expect, "upper block placed");
    if (override.empty()) {
        ctx.check(bs::game::intState(region->getBlock(pos), bs::ids::kStateFacing) == 1, "facing state");
        ctx.check(bs::game::intState(region->getBlock(BlockPos{pos.x, pos.y + 1, pos.z}), bs::ids::kStateUpper) == 1, "upper state");
    }
    auto again = wg::placeWaystone(*region, pos, "waystone", 1, bs::core::Origin::Wilderness, nullptr, override);
    ctx.check(again == wg::PlaceResult::Occupied, "a second waystone is not placed on the same spot");

    // The surface finder lands on the platform.
    auto surface = wg::wildSurfaceY(*region, center.x + 1, center.z, false);
    ctx.check(surface && *surface == y, "wild surface is the top solid block + 1");

    // Village: a bell on a platform gets a VILLAGE waystone 4-12 blocks away.
    auto vc = ctx.testPos(1550);
    ctx.require(co_await loadArea(ctx, vc), "village area loaded");
    int vy = buildPlatform(ctx, *region, vc, 6);
    BlockPos bell{vc.x, vy, vc.z};
    ctx.setBlock({0, bell.x, bell.y, bell.z}, "minecraft:bell");
    auto found = wg::findBell(*region, bell.x >> 4, bell.z >> 4);
    ctx.check(found && *found == bell, "bell found in the chunk");
    auto spot = wg::villageSpot(*region, bell);
    ctx.require(spot.has_value(), "village spot found");
    int d2 = (spot->x - bell.x) * (spot->x - bell.x) + (spot->z - bell.z) * (spot->z - bell.z);
    ctx.check(d2 >= 16 && d2 <= 144 && spot->y == vy, "village spot near the centre on the platform");

    auto& cfg   = bs::config::mut<wg::WorldGenConfig>();
    auto  mode  = cfg.spawnInVillages;
    cfg.spawnInVillages = "FREQUENT";
    auto oldTest = wg::testBlock();
    if (!override.empty()) wg::setTestBlock(override);
    std::string vuid;
    auto        vres = wg::generateVillage(0, bell, &vuid);
    wg::setTestBlock(oldTest);
    cfg.spawnInVillages = mode;
    wg::forgetVillage(cw::villageKey(0, bell.x, bell.z));
    ctx.require(vres == wg::PlaceResult::Placed, "village waystone placed (" + std::string(wg::toString(vres)) + ")");
    ctx.cleanupWaystone(vuid);
    auto* vrec = bs::game::Waystones::get().find(vuid);
    ctx.require(vrec != nullptr, "village record");
    ctx.cleanupBlock(vrec->pos);
    ctx.cleanupBlock({0, vrec->pos.x, vrec->pos.y + 1, vrec->pos.z});
    ctx.check(vrec->origin == bs::core::Origin::Village, "origin VILLAGE");
    ctx.check(vrec->name.empty() && !vrec->hasOwner(), "village waystone has no name / owner");
});

BS_REGISTER_SELFTEST("worldgen.dimension_filter", [](Context& ctx) {
    auto& cfg   = bs::config::mut<wg::WorldGenConfig>();
    auto  saved = cfg;
    cfg.wildWaystonesDimensionAllowList = {"minecraft:overworld", "minecraft:the_nether", "minecraft:the_end"};
    cfg.wildWaystonesDimensionDenyList  = {};
    cfg.chunksBetweenWildWaystones      = 25;
    ctx.check(wg::wildEnabledIn(0) && wg::wildEnabledIn(1) && wg::wildEnabledIn(2), "default allow list");
    cfg.wildWaystonesDimensionDenyList = {"the_nether"};
    ctx.check(wg::wildEnabledIn(0) && !wg::wildEnabledIn(1), "deny list wins");
    cfg.wildWaystonesDimensionDenyList  = {};
    cfg.wildWaystonesDimensionAllowList = {"overworld"};
    ctx.check(wg::wildEnabledIn(0) && !wg::wildEnabledIn(2), "allow list restricts");
    cfg.wildWaystonesDimensionAllowList = {};
    ctx.check(wg::wildEnabledIn(2), "empty allow list allows all");
    cfg.chunksBetweenWildWaystones = 0;
    ctx.check(!wg::wildEnabledIn(0), "0 chunks disables wild waystones");
    cw::WildSpot spot = cw::wildSpot(wg::worldSeed(), {7777, 7777}, 25);
    ctx.check(wg::generateWild(0, spot) == wg::PlaceResult::Disabled, "generation skipped when disabled");
    cfg = saved;
});

BS_REGISTER_SELFTEST("worldgen.persistence", [](Context& ctx) {
    std::string cell    = cw::cellKey(0, 25, {-99999, 99999});
    std::string village = cw::villageKey(0, -999999, 999999);
    wg::markCellDone(cell);
    ctx.check(wg::isCellDone(cell), "cell marked done");
    wg::saveState();
    wg::reloadState();
    ctx.check(wg::isCellDone(cell), "done cell survives a reload from the world DB");
    ctx.check(!wg::isVillageDone(village), "unknown village is not done");
    wg::forgetCell(cell);
    wg::saveState();
    wg::reloadState();
    ctx.check(!wg::isCellDone(cell), "forgotten cell stays forgotten");
});
