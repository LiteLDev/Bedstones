#include "feature/waystone/WaystoneBlocks.h"

#include <cmath>
#include <map>
#include <random>
#include <set>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerDestroyBlockEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "ll/api/event/player/PlayerPlaceBlockEvent.h"
#include "ll/api/event/world/BlockChangedEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/network/MinecraftPacketIds.h"
#include "mc/network/MinecraftPackets.h"
#include "mc/network/packet/SpawnParticleEffectPacket.h"
#include "mc/world/Facing.h"
#include "mc/world/actor/player/AbilitiesIndex.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ItemTag.h"
#include "mc/world/item/VanillaItemTags.h"
#include "mc/world/item/enchanting/Enchant.h"
#include "mc/world/item/enchanting/EnchantUtils.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/Spawner.h"
#include "mc/world/level/biome/Biome.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockType.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/phys/AABB.h"

#include "game/BlockContainer.h"
#include "game/Blocks.h"
#include "game/Effects.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "service/Services.h"

namespace bs::feature::waystone {

namespace {

using core::GlobalPos;
using core::WaystoneRecord;

constexpr std::int64_t kUseCooldownTicks = 8;  ///< holding "use" repeats the interact event
constexpr std::int64_t kBreakInfoTicks   = 40; ///< how long a noted breaker stays valid
constexpr float        kAmbientRange     = 32.f;

/// Who broke a waystone (from PlayerDestroyBlockEvent), keyed by the lower half.
struct BreakInfo {
    std::string  player;
    bool         silk     = false;
    bool         tool     = false;
    bool         creative = false;
    std::int64_t tick     = 0;
};

/// A waystone block went away (either half); handled on the next tick.
struct Removal {
    GlobalPos   lower;
    std::string blockId;
};

/// Deferred right-click (place or use), run on the next tick outside the interact transaction.
struct Action {
    enum class Kind { Place, Use };
    Kind          kind;
    std::int64_t  player;
    GlobalPos     pos;
    int           face   = 1;
    bool          sneak  = false;
    std::string   heldId;
};

struct State {
    std::int64_t                          now = 0;
    int                                   guard = 0; ///< our own block edits: ignore block-changed events
    std::map<GlobalPos, BreakInfo>        breakers;
    std::vector<Removal>                  removals;
    std::vector<Action>                   actions;
    std::map<std::int64_t, std::int64_t>  lastUse; ///< player unique id -> tick of the last handled click
    std::vector<ItemStack>                lastDrops;
};

State& state() {
    static State s;
    return s;
}

struct Guard {
    Guard() { ++state().guard; }
    ~Guard() { --state().guard; }
    Guard(Guard const&)            = delete;
    Guard& operator=(Guard const&) = delete;
};

std::mt19937& rng() {
    static std::mt19937 r{std::random_device{}()};
    return r;
}

float rand01() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng()); }

BlockSource* regionOf(int dim) {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    auto d = level->getDimension(dim).lock();
    if (!d) return nullptr;
    return &d->getBlockSourceFromMainChunkSource();
}

Player* playerById(std::int64_t raw) {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    ActorUniqueID id;
    id.rawID = raw;
    return level->getPlayer(id);
}

BlockPos above(BlockPos const& p) { return BlockPos{p.x, p.y + 1, p.z}; }

bool isPickaxe(ItemStack const& item) {
    if (item.isNull()) return false;
    if (item.hasTag(VanillaItemTags::Pickaxe())) return true;
    return game::idOf(item).ends_with("_pickaxe");
}

bool hasSilkTouch(ItemStack const& item) {
    return !item.isNull() && EnchantUtils::getEnchantLevel(Enchant::Type::SilkTouch, item) > 0;
}

/// Generated waystones are named on first activation (Waystones names wilderness / village ones;
/// command-set blocks without a record are treated the same so they never stay nameless).
bool isGenerated(core::Origin o) { return o != core::Origin::Player; }

std::string biomeName(BlockSource& region, BlockPos const& pos) {
    try {
        return region.getBiome(pos).mHash->getString();
    } catch (...) {
        return {};
    }
}

/// One particle effect sent to one player only (the runes / ambient particles are per viewer).
void particleFor(Player& player, std::string const& name, Vec3 const& at) {
    auto  pkt              = MinecraftPackets::createPacket(MinecraftPacketIds::SpawnParticleEffect);
    auto& p                = static_cast<SpawnParticleEffectPacket&>(*pkt);
    p.mVanillaDimensionId  = static_cast<uchar>(static_cast<int>(player.getDimensionId()));
    p.mActorId->rawID      = -1;
    p.mPos                 = at;
    p.mEffectName          = name;
    p.sendTo(player);
}

/// Adjacent observers looking at either half get a pulse (Waystones schedules them a tick).
void pulseObservers(BlockSource& region, BlockPos const& lower) {
    for (auto const& cell : {lower, above(lower)}) {
        for (auto const& dir : Facing::DIRECTION()) {
            BlockPos nb{cell.x + dir.x, cell.y + dir.y, cell.z + dir.z};
            if (!region.hasChunksAt(nb, 0, false)) continue;
            auto const& b = region.getBlock(nb);
            if (b.getTypeName() != "minecraft:observer") continue;
            b.getBlockType().neighborChanged(region, nb, cell);
        }
    }
}

void spawnDrop(BlockSource& region, BlockPos const& pos, ItemStack const& stack) {
    if (stack.isNull()) return;
    state().lastDrops.push_back(stack);
    region.getLevel().getSpawner().spawnItem(region, stack, nullptr, pos.center(), 10);
}

std::string displayName(WaystoneRecord const& r) { return r.name; }

// ---- removal ------------------------------------------------------------------------------------

void handleRemoval(Removal const& rm) {
    auto& s      = state();
    auto* region = regionOf(rm.lower.dim);
    if (!region) return;
    auto lower = game::blockPos(rm.lower);
    // Neither half survives without the other (a different block set in its place stays).
    {
        Guard g;
        for (auto const& cell : {lower, above(lower)}) {
            if (region->hasChunksAt(cell, 0, false) && region->getBlock(cell).getTypeName() == rm.blockId) {
                game::clearBlock(*region, cell);
            }
        }
    }
    std::optional<BreakInfo> info;
    if (auto it = s.breakers.find(rm.lower); it != s.breakers.end()) {
        if (s.now - it->second.tick <= kBreakInfoTicks) info = it->second;
        s.breakers.erase(it);
    }
    auto& ws  = game::Waystones::get();
    auto* rec = ws.findAt(rm.lower);
    if (rec && rec->transient) rec = nullptr; // a silk-touched one already in transit
    bool drops = info && !info->creative && info->tool;
    bool  keep  = drops && info->silk && rec && rec->isWaystone();
    s.lastDrops.clear();
    if (rec) {
        // The modifier container always spills (its items must not vanish with the block).
        auto container = game::containerOf(*rec);
        bool any       = false;
        for (auto const& slot : container) {
            if (!slot) continue;
            spawnDrop(*region, lower, game::toStack(*slot));
            any = true;
        }
        if (any) {
            game::setContainer(*rec, game::Container{});
            ws.markDirty(rec->uid);
        }
    }
    if (drops) spawnDrop(*region, lower, keep ? silkItem(*rec) : game::makeItem(rm.blockId));
    if (!rec) return;
    auto uid  = rec->uid;
    auto name = rec->name;
    if (keep) {
        rec->transient = true;
        ws.markDirty(uid);
        log().info("waystone picked up with silk touch: {} ({}) by {}", name, uid, info->player);
        return;
    }
    // Every player forgets it: offline players' data is pruned of removed uids when it is next
    // read; online players forget it now so change listeners (sync, open menus) see it.
    if (auto level = ll::service::getLevel()) {
        level->forEachPlayer([&](Player& p) {
            if (!p.isClientSide()) ws.forget(p, uid);
            return true;
        });
    }
    ws.remove(uid);
    log().info("waystone removed: {} ({}){}", name, uid, info ? " by " + info->player : std::string{});
}

// ---- placement ----------------------------------------------------------------------------------

void placeFromHand(Player& player, Action const& a) {
    if (!player.canUseAbility(AbilitiesIndex::Build)) return;
    auto* region = &player.getDimensionBlockSource();
    if (static_cast<int>(player.getDimensionId()) != a.pos.dim) return;
    auto loc  = game::handSlot(player, 0);
    auto item = game::itemAt(player, loc); // copy: the stack may change below
    if (game::idOf(item) != a.heldId || !isWaystoneId(a.heldId)) return;
    // `pos` is the cell the game was about to fill, or the clicked block when that is solid.
    auto clicked = game::blockPos(a.pos);
    auto target  = clicked;
    if (isWaystoneBlock(region->getBlock(clicked)) || !game::replaceable(*region, clicked)) {
        if (a.face < 0 || a.face > 5) return;
        auto const& d = Facing::DIRECTION()[static_cast<std::size_t>(a.face)];
        target        = BlockPos{clicked.x + d.x, clicked.y + d.y, clicked.z + d.z};
    }
    // The placer's distance is validated by the game (it sent the use); re-check it loosely anyway.
    auto const& pp = player.getPosition();
    float dx = pp.x - (static_cast<float>(target.x) + 0.5f), dy = pp.y - static_cast<float>(target.y), dz = pp.z - (static_cast<float>(target.z) + 0.5f);
    if (dx * dx + dy * dy + dz * dz > 12.f * 12.f) return;
    auto res = place(player, *region, target, item);
    if (res.result != PlaceResult::Ok) return;
    game::consumeAt(player, loc, 1, true);
}

} // namespace

// ---- public API ------------------------------------------------------------------------------------

bool isWaystoneId(std::string_view fullId) {
    auto path = ids::pathOf(fullId);
    return !path.empty() && ids::isWaystoneStyle(path);
}

bool isWaystoneBlock(Block const& block) { return isWaystoneId(block.getTypeName()); }

Placed place(Player& player, BlockSource& region, BlockPos const& pos, ItemStack const& item, std::optional<int> facing, bool openEdit) {
    Placed out;
    auto   id = game::idOf(item);
    if (!isWaystoneId(id)) {
        out.result = PlaceResult::NotWaystone;
        return out;
    }
    auto& ws = game::Waystones::get();
    if (!ws.available()) return out;
    auto up = above(pos);
    if (!game::replaceable(region, pos) || !game::replaceable(region, up)
        || !region.isUnobstructedByEntities(
            AABB(Vec3{static_cast<float>(pos.x), static_cast<float>(pos.y), static_cast<float>(pos.z)},
                 Vec3{static_cast<float>(pos.x + 1), static_cast<float>(pos.y + 2), static_cast<float>(pos.z + 1)}),
            nullptr
        )) {
        out.result = PlaceResult::NoRoom;
        return out;
    }
    int f = facing.value_or(game::opposite(game::playerFacing(player))) & 3;
    {
        Guard g;
        if (!game::placeTwoTall(region, pos, id, f)) return out;
    }
    auto gpos  = game::globalPos(region, pos);
    auto style = std::string(ids::pathOf(id));
    WaystoneRecord* rec = nullptr;
    if (auto uid = game::userString(item, ids::kNbtWaystone)) {
        // Silk-touched item: bring its record back. A record that is not transient belongs to a
        // waystone that may still stand (a copied item): only take it over when its block is
        // known to be gone; otherwise this is a new waystone.
        if (auto* existing = ws.find(*uid); existing && existing->isWaystone()) {
            bool gone = existing->transient;
            if (!gone) {
                if (auto* r = regionOf(existing->pos.dim)) {
                    auto p = game::blockPos(existing->pos);
                    gone   = r->hasChunksAt(p, 0, false) && !isWaystoneBlock(r->getBlock(p));
                }
            }
            if (gone) rec = existing;
        }
    }
    // A record indexed at this position: a stale one (its block vanished without an event) is
    // removed; a transient one stays in transit with its item.
    if (auto* old = ws.findAt(gpos); old && old != rec && !old->transient) ws.remove(std::string(old->uid));
    if (rec) {
        auto uid = rec->uid;
        // The registry indexes positions by record: moving away from a cell another record now
        // occupies would drop that one's index entry, so re-index it afterwards.
        std::string occupant;
        if (auto* o = ws.findAt(rec->pos); o && o != rec) occupant = o->uid;
        rec->transient = false;
        rec->facing    = f;
        rec->style     = style;
        ws.move(uid, gpos); // persists + notifies
        if (auto* o = occupant.empty() ? nullptr : ws.find(occupant)) ws.move(occupant, o->pos);
        rec          = ws.find(uid);
        out.restored = true;
    }
    if (!rec) {
        WaystoneRecord r;
        r.type       = std::string(core::types::kWaystone);
        r.style      = style;
        r.pos        = gpos;
        r.facing     = f;
        r.origin     = core::Origin::Player;
        r.ownerUid   = game::Waystones::playerKey(player);
        r.ownerName  = player.getRealName();
        r.visibility = service::get<service::IPermissions>().defaultVisibility();
        rec          = ws.create(std::move(r));
        if (!rec) {
            Guard g;
            game::clearBlock(region, up);
            game::clearBlock(region, pos);
            return out;
        }
    }
    out.uid    = rec->uid;
    out.result = PlaceResult::Ok;
    ws.activate(player, out.uid);
    log().info(
        "waystone {}: {} ({}) at {} {} {} facing {} by {}",
        out.restored ? "restored" : "placed",
        rec->name,
        out.uid,
        pos.x,
        pos.y,
        pos.z,
        f,
        player.getRealName()
    );
    if (openEdit) {
        log().info("waystone edit opened: {} ({}) by {}", rec->name, out.uid, player.getRealName());
        service::get<service::IEditScreen>().open(player, out.uid);
    }
    return out;
}

WaystoneRecord* recordAt(BlockSource& region, BlockPos const& pos, bool create) {
    if (!region.hasChunksAt(pos, 0, false)) return nullptr;
    auto const& block = region.getBlock(pos);
    if (!isWaystoneBlock(block)) return nullptr;
    auto  lower = game::lowerHalf(region, pos);
    auto  gpos  = game::globalPos(region, lower);
    auto& ws    = game::Waystones::get();
    // A transient record indexed here is a silk-touched waystone in transit, not this block's.
    if (auto* r = ws.findAt(gpos); r && !r->transient) return r;
    if (!create || !ws.available()) return nullptr;
    WaystoneRecord r;
    r.type   = std::string(core::types::kWaystone);
    r.style  = std::string(ids::pathOf(block.getTypeName()));
    r.pos    = gpos;
    r.facing = game::intState(block, ids::kStateFacing).value_or(0);
    r.origin = core::Origin::Unknown;
    auto* created = ws.create(std::move(r));
    if (created) log().info("waystone initialized: {} at {} {} {}", created->uid, lower.x, lower.y, lower.z);
    return created;
}

bool activate(Player& player, BlockSource& region, WaystoneRecord& record) {
    auto& ws  = game::Waystones::get();
    auto  uid = record.uid;
    if (ws.isActivated(player, uid)) return false;
    auto pos = game::blockPos(record.pos);
    if (record.name.empty() && isGenerated(record.origin)) {
        record.name = service::get<service::INameGenerator>().generate(record.origin, biomeName(region, pos));
        if (!record.hasOwner()) {
            record.ownerUid   = game::Waystones::playerKey(player);
            record.ownerName  = player.getRealName();
            record.visibility = service::get<service::IPermissions>().defaultVisibility();
        }
        ws.markDirty(uid);
    }
    auto name = record.name; // the record may move in memory once listeners run
    ws.activate(player, uid);
    game::chat(player, "§e" + trFor(player, "chat.waystones.waystone_activated", "Activated Waystone: {}", "§f" + name));
    int  dim = record.pos.dim;
    Vec3 center{static_cast<float>(pos.x) + 0.5f, static_cast<float>(pos.y) + 0.5f, static_cast<float>(pos.z) + 0.5f};
    game::soundAround(dim, "random.levelup", center, 0.2f, 1.f);
    for (int dy : {3, 4}) {
        for (int i = 0; i < 32; ++i) {
            game::particle(
                dim,
                "minecraft:enchanting_table_particle",
                Vec3{center.x + (rand01() - 0.5f) * 2.f, static_cast<float>(pos.y + dy), center.z + (rand01() - 0.5f) * 2.f}
            );
        }
    }
    pulseObservers(region, pos);
    log().info("waystone activated: {} by {}", name, player.getRealName());
    return true;
}

void use(Player& player, BlockSource& region, BlockPos const& pos, bool sneaking, std::string const& heldId) {
    auto* rec = recordAt(region, pos, true);
    if (!rec) return;
    auto uid = rec->uid;
    if (player.isCreative() && heldId == "minecraft:bamboo") {
        // Hidden debug tool: forget the waystone and initialize it again.
        auto copy = *rec;
        auto& ws  = game::Waystones::get();
        ws.remove(uid);
        WaystoneRecord fresh;
        fresh.type   = copy.type;
        fresh.style  = copy.style;
        fresh.pos    = copy.pos;
        fresh.facing = copy.facing;
        fresh.origin = copy.origin;
        fresh.extra  = copy.extra; // block data (modifier container) belongs to the block, not the record
        ws.create(std::move(fresh));
        game::chat(player, trFor(player, "bs.waystone.reset", "Waystone was successfully reset - it will re-initialize once it is next loaded."));
        log().info("waystone reset: {} by {}", uid, player.getRealName());
        return;
    }
    if (player.isCreative() && heldId == "minecraft:stick") {
        game::chat(player, trFor(player, "bs.waystone.server_uid", "Server UUID: {}", uid));
        return;
    }
    if (sneaking) {
        log().info("waystone edit opened: {} ({}) by {}", rec->name, uid, player.getRealName());
        service::get<service::IEditScreen>().open(player, uid);
        return;
    }
    if (!game::Waystones::get().isActivated(player, uid)) {
        activate(player, region, *rec);
        return;
    }
    core::WarpSource source;
    source.kind    = core::WarpSourceKind::Waystone;
    source.fromUid = uid;
    source.fromPos = rec->pos;
    log().info("waystone selection opened: {} by {}", rec->name, player.getRealName());
    service::get<service::ISelection>().open(player, source);
}

void noteBreak(Player& player, BlockSource& region, BlockPos const& pos) {
    if (!isWaystoneBlock(region.getBlock(pos))) return;
    auto const& tool = player.getSelectedItem();
    BreakInfo   info;
    info.player   = player.getRealName();
    info.silk     = hasSilkTouch(tool);
    info.tool     = isPickaxe(tool);
    info.creative = player.isCreative();
    info.tick     = state().now;
    state().breakers[game::globalPos(region, game::lowerHalf(region, pos))] = info;
}

ItemStack silkItem(WaystoneRecord const& record) {
    auto stack = game::makeItem(ids::id(record.style.empty() ? "waystone" : record.style));
    game::setUserString(stack, ids::kNbtWaystone, record.uid);
    if (!record.name.empty()) game::setDisplay(stack, {}, {"§b" + displayName(record)});
    return stack;
}

void flush() {
    auto& s = state();
    if (!s.actions.empty()) {
        auto actions = std::move(s.actions);
        s.actions.clear();
        for (auto const& a : actions) {
            auto* player = playerById(a.player);
            if (!player || !player->isAlive()) continue;
            if (a.kind == Action::Kind::Place) {
                placeFromHand(*player, a);
            } else if (static_cast<int>(player->getDimensionId()) == a.pos.dim) {
                use(*player, player->getDimensionBlockSource(), game::blockPos(a.pos), a.sneak, a.heldId);
            }
        }
    }
    if (!s.removals.empty()) {
        auto removals = std::move(s.removals);
        s.removals.clear();
        std::set<GlobalPos> done;
        for (auto const& rm : removals) {
            if (done.insert(rm.lower).second) handleRemoval(rm);
        }
    }
    std::erase_if(s.breakers, [&](auto const& kv) { return s.now - kv.second.tick > kBreakInfoTicks; });
}

std::vector<ItemStack> const& lastDrops() { return state().lastDrops; }

// ---- module -----------------------------------------------------------------------------------------

namespace {

/// Ambient particles of activated waystones, per viewer (Waystones' `animateTick`: portal + enchant
/// particles around the base, 75% of the client's random display ticks).
void ambientParticles() {
    auto level = ll::service::getLevel();
    if (!level) return;
    auto& ws = game::Waystones::get();
    if (!ws.available()) return;
    auto records = ws.ofType(core::types::kWaystone);
    if (records.empty()) return;
    level->forEachPlayer([&](Player& p) {
        if (p.isClientSide()) return true;
        int   dim = static_cast<int>(p.getDimensionId());
        auto  pos = p.getPosition();
        auto& data = ws.playerData(p);
        for (auto* r : records) {
            if (r->transient || r->pos.dim != dim) continue;
            Vec3 c{static_cast<float>(r->pos.x) + 0.5f, static_cast<float>(r->pos.y) + 0.5f, static_cast<float>(r->pos.z) + 0.5f};
            if (pos.distanceTo(c) > kAmbientRange || !data.isActivated(r->uid)) continue;
            // A block gets a random display tick roughly every 25 ticks; we run every 5.
            if (rand01() >= 0.75f * 5.f / 25.f) continue;
            auto jitter = [] { return (rand01() - 0.5f) * 1.5f; };
            particleFor(p, "minecraft:portal_directional", Vec3{c.x + jitter(), c.y, c.z + jitter()});
            particleFor(p, "minecraft:enchanting_table_particle", Vec3{c.x + jitter(), c.y, c.z + jitter()});
        }
        return true;
    });
}

/// Queues a click for the next tick; holding "use" repeats the event, one action per click.
void queueAction(Player& player, Action::Kind kind, GlobalPos const& pos, int face, bool sneak, std::string const& heldId) {
    auto& s   = state();
    auto  key = player.getOrCreateUniqueID().rawID;
    if (auto it = s.lastUse.find(key); it != s.lastUse.end() && s.now - it->second < kUseCooldownTicks) return;
    s.lastUse[key] = s.now;
    s.actions.push_back(Action{kind, key, pos, face, sneak, heldId});
}

} // namespace

class WaystoneBlocks {
public:
    static WaystoneBlocks& get() {
        static WaystoneBlocks instance;
        return instance;
    }

    void install() {
        auto& bus = ll::event::EventBus::getInstance();
        mListeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([](auto&) {
            auto& s = state();
            ++s.now;
            flush();
            if (s.now % 5 == 0) ambientParticles();
            if (s.now % 200 == 0) std::erase_if(s.lastUse, [&](auto const& kv) { return s.now - kv.second > kUseCooldownTicks; });
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::BlockChangedEvent>([](ll::event::BlockChangedEvent& ev) {
            if (state().guard > 0 || ev.layer() != 0) return;
            auto& region = ev.blockSource();
            if (region.getLevel().isClientSide()) return;
            auto const& prev = ev.previousBlock();
            if (!isWaystoneBlock(prev)) return;
            if (ev.newBlock().getTypeName() == prev.getTypeName()) return; // state change only
            auto const& pos   = ev.pos();
            bool        upper = game::intState(prev, ids::kStateUpper).value_or(0) != 0;
            BlockPos    lower = upper ? BlockPos{pos.x, pos.y - 1, pos.z} : pos;
            state().removals.push_back({game::globalPos(region, lower), prev.getTypeName()});
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::PlayerDestroyBlockEvent>(
            [](ll::event::PlayerDestroyBlockEvent& ev) {
                auto& player = ev.self();
                if (player.isClientSide() || ev.isCancelled()) return;
                noteBreak(player, player.getDimensionBlockSource(), ev.pos());
            },
            ll::event::EventPriority::Low
        ));
        mListeners.push_back(bus.emplaceListener<ll::event::PlayerInteractBlockEvent>(
            [](ll::event::PlayerInteractBlockEvent& ev) {
                auto& player = ev.self();
                if (player.isClientSide() || ev.isCancelled()) return;
                auto&       region = player.getDimensionBlockSource();
                auto const& pos    = ev.blockPos();
                if (!isWaystoneBlock(region.getBlock(pos))) return;
                auto heldId  = game::idOf(player.getSelectedItem());
                bool sneak   = player.isSneaking();
                bool hasItem = !player.getSelectedItem().isNull() || !player.getOffhandSlot().isNull();
                // Vanilla: sneaking with an item in hand skips the block's interaction (the item is
                // used / placed instead).
                if (sneak && hasItem) return;
                if (heldId == ids::id(ids::kBlankScroll)) return; // the scroll's own use handles it
                ev.cancel();
                queueAction(player, Action::Kind::Use, game::globalPos(region, pos), static_cast<int>(ev.face()), sneak, heldId);
            },
            ll::event::EventPriority::High
        ));
        // Placing a waystone item: the game would place only the default (lower) state; both halves
        // are placed on the next tick instead.
        mListeners.push_back(bus.emplaceListener<ll::event::PlayerPlacingBlockEvent>(
            [](ll::event::PlayerPlacingBlockEvent& ev) {
                auto& player = ev.self();
                if (player.isClientSide() || ev.isCancelled()) return;
                auto heldId = game::idOf(player.getSelectedItem());
                if (!isWaystoneId(heldId)) return;
                ev.cancel();
                auto& region = player.getDimensionBlockSource();
                queueAction(
                    player,
                    Action::Kind::Place,
                    game::globalPos(region, ev.pos()),
                    static_cast<int>(ev.face()),
                    player.isSneaking(),
                    heldId
                );
            },
            ll::event::EventPriority::High
        ));
    }

    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
        auto& s = state();
        s.breakers.clear();
        s.removals.clear();
        s.actions.clear();
        s.lastUse.clear();
        s.lastDrops.clear();
    }

private:
    std::vector<ll::event::ListenerPtr> mListeners;
};

} // namespace bs::feature::waystone

BS_REGISTER_MODULE(bs::feature::waystone::WaystoneBlocks);
