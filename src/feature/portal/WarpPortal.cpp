// Warp Portal: spawned by the Portal Scroll interceptor, warps entities walking into it, expires
// after 6000 ticks, and is offered as "Return to Portal" in selection menus (docs/SPEC.md §1.6).

#include "feature/portal/Portal.h"
#include "feature/memorial/Memorial.h"
#include "feature/portal/World.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/block/BlockSupportType.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionType.h"
#include "mc/world/phys/AABB.h"

#include "game/Blocks.h"
#include "game/Effects.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "game/WorldDb.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "service/Services.h"
#include "service/TargetProviders.h"
#include "service/WarpEvents.h"

namespace bs::feature::portal {

namespace {

using core::WarpSourceKind;

struct PortalState {
    int ticksLeft = core::portal::kPortalLifetimeTicks;
};

std::unordered_map<std::string, PortalState>   gPortals;   ///< live countdowns by record uid
std::unordered_map<std::int64_t, std::uint64_t> gCooldowns; ///< actor unique id -> first tick it may use a portal again
std::uint64_t                                   gTick = 0;

std::string portalId() { return ids::id(ids::kWarpPortal); }

Vec3 center(core::GlobalPos const& p, float dy = 1.0f) {
    return Vec3{static_cast<float>(p.x) + 0.5f, static_cast<float>(p.y) + dy, static_cast<float>(p.z) + 0.5f};
}

PortalState& stateOf(core::WaystoneRecord const& r) {
    auto it = gPortals.find(r.uid);
    if (it == gPortals.end()) {
        PortalState s;
        if (r.extra.contains(kExtraTicksLeft) && r.extra[kExtraTicksLeft].is_number_integer()) {
            s.ticksLeft = r.extra[kExtraTicksLeft].get<int>();
        }
        it = gPortals.emplace(r.uid, s).first;
    }
    return it->second;
}

/// Writes the live countdowns into the records (autosave / world stop).
void persistCountdowns() {
    auto& ws = game::Waystones::get();
    if (!ws.available()) return;
    for (auto const& [uid, st] : gPortals) {
        auto* r = ws.find(uid);
        if (!r) continue;
        if (r->extra.value(kExtraTicksLeft, -1) == st.ticksLeft) continue;
        r->extra[kExtraTicksLeft] = st.ticksLeft;
        ws.markDirty(uid);
    }
}

/// Source kinds whose menus list the return portal (docs/SPEC.md §3.7).
bool listsReturnPortal(WarpSourceKind k) {
    switch (k) {
    case WarpSourceKind::Waystone:
    case WarpSourceKind::Sharestone:
    case WarpSourceKind::WarpScroll:
    case WarpSourceKind::WarpStone:
        return true;
    default:
        return false;
    }
}

core::WaystoneRecord* portalRecord(std::string const& uid) {
    auto* r = game::Waystones::get().find(uid);
    return r && r->type == core::types::kWarpPortal ? r : nullptr;
}

/// Waystones' `isValid()` of a portal target: its record (or provider entry) still exists.
bool targetExists(std::string const& target) {
    return !target.empty() && (game::Waystones::get().find(target) || service::providerFor(target));
}

/// Targets non-player entities may be sent to: plain, non-transient waystone-like blocks (not
/// another player's memorial or portal; those resolve per player).
bool entityTarget(core::WaystoneRecord const& r) {
    return !r.transient && r.type != core::types::kFleetingMemorial && r.type != core::types::kWarpPortal;
}

void showError(Player& player, std::string const& key, std::string const& fallback) {
    game::actionBar(player, "§4" + trFor(player, key, fallback));
}

/// "There is no space to open a Warp Portal here." (dark red chat line, like Waystones).
service::WarpResult noSpace(Player& player) {
    showNoSpace(player);
    return service::WarpResult::fail("chat.waystones.warp_portal_no_space", "There is no space to open a Warp Portal here.");
}

// ---- entering ----------------------------------------------------------------------------------

void enterPortal(Actor& actor, std::string const& uid) {
    auto* r = portalRecord(uid);
    if (!r) return;
    std::string target = r->extra.value(kExtraTarget, std::string{});
    auto        from   = r->pos;
    if (!targetExists(target)) {
        if (actor.isPlayer()) {
            auto& player = static_cast<Player&>(actor);
            game::chat(player, trFor(player, "chat.waystones.warp_portal_attunement_lost", "This portal has closed because its target was destroyed."));
        }
        removePortal(uid, true, "target lost");
        return;
    }
    if (actor.isPlayer()) {
        auto&                player = static_cast<Player&>(actor);
        auto&                tp     = service::get<service::ITeleport>();
        service::WarpRequest req;
        req.source.kind    = WarpSourceKind::WarpPortal;
        req.source.fromUid = uid;
        req.source.fromPos = from;
        req.targetId       = target;
        auto res           = tp.warp(player, req);
        if (!res.ok) return;
        auto& ws                              = game::Waystones::get();
        ws.playerData(player).extra[kExtraReturnPortal] = uid;
        ws.markPlayerDirty(game::Waystones::playerKey(player));
        log().info("warp portal used: {} -> {} by {}", uid, target, player.getRealName());
        return;
    }
    // Other entities (mobs, items): plain waystone-like targets only (ITeleport takes players).
    auto* dest = game::Waystones::get().find(target);
    if (!dest || !entityTarget(*dest)) return;
    auto d = landingNear(*dest);
    actor.teleport(Vec3{d.x, d.y, d.z}, DimensionType(d.block.dim), Vec2{0.f, d.yaw});
}

void tickPortal(BlockSource& region, std::string const& uid) {
    auto* r = portalRecord(uid);
    if (!r) return;
    BlockPos lower = game::blockPos(r->pos);
    AABB     box{
        Vec3{static_cast<float>(lower.x), static_cast<float>(lower.y), static_cast<float>(lower.z)},
        Vec3{static_cast<float>(lower.x + 1), static_cast<float>(lower.y + 2), static_cast<float>(lower.z + 1)}
    };
    for (Actor* actor : entitiesIn(region, box)) {
        if (!actor || !actor->isAlive() || actor->getTypeName() == memorial::kLabelEntity) continue;
        if (!actor->getAABB().intersects(box)) continue;
        auto id = actor->getOrCreateUniqueID().rawID;
        if (auto it = gCooldowns.find(id); it != gCooldowns.end() && it->second > gTick) continue;
        gCooldowns[id] = gTick + core::portal::kPortalReuseCooldownTicks;
        enterPortal(*actor, uid);
        if (!portalRecord(uid)) return; // closed while warping
    }
}

void tick() {
    ++gTick;
    auto& ws = game::Waystones::get();
    if (!ws.available()) return;
    // Portals are tracked in gPortals; records loaded from the world are picked up by a rescan.
    if ((gTick % 100) == 1) {
        for (auto* r : ws.ofType(core::types::kWarpPortal)) stateOf(*r);
    }
    if (gPortals.empty()) return;
    std::vector<std::string> uids;
    for (auto const& [uid, st] : gPortals) uids.push_back(uid);
    for (auto const& uid : uids) {
        auto* r = portalRecord(uid);
        if (!r) {
            gPortals.erase(uid);
            continue;
        }
        auto* region = regionFor(r->pos.dim);
        if (!region) continue;
        BlockPos lower = game::blockPos(r->pos);
        if (!region->hasChunksAt(lower, 0, false)) continue; // frozen while unloaded (like a block entity)
        if (game::blockIdAt(*region, lower) != portalId()) {
            removePortal(uid, false, "block gone");
            continue;
        }
        if (--stateOf(*r).ticksLeft <= 0) {
            removePortal(uid, true, "expired");
            continue;
        }
        // Idle particles rising from the bottom of the portal.
        if ((gTick % 5) == 0) {
            game::particles(r->pos.dim, "minecraft:mob_portal", center(r->pos, 0.1f), 1, 0.4f);
        }
        tickPortal(*region, uid);
    }
    if ((gTick % 200) == 0) std::erase_if(gCooldowns, [](auto const& e) { return e.second <= gTick; });
}

// ---- interceptor -------------------------------------------------------------------------------

std::optional<service::WarpResult> interceptPortalScroll(Player& player, service::WarpRequest const& request) {
    if (request.source.kind != WarpSourceKind::PortalScroll) return std::nullopt;
    auto fail = [&](std::string key, std::string fallback) {
        showError(player, key, fallback);
        return service::WarpResult::fail(std::move(key), std::move(fallback));
    };
    auto loc = game::handSlot(player, request.source.hand);
    if (game::idOf(game::itemAt(player, loc)) != ids::id(ids::kPortalScroll)) {
        return fail("bs.portal.scroll_missing", "You need to hold the Portal Scroll.");
    }
    // The target must be one the Portal Scroll menu offers (never trust the client).
    auto targets = service::get<service::ITargets>().list(player, request.source);
    auto it      = std::find_if(targets.begin(), targets.end(), [&](auto const& t) { return t.id == request.targetId && !t.current; });
    if (it == targets.end() || !service::get<service::ITeleport>().resolve(player, request.targetId)) {
        return fail("chat.waystones.waystone_missing", "The waystone you tried to teleport to is currently being moved or has gone missing.");
    }
    auto spot = portalSpotFor(player);
    if (!spot) return noSpace(player);
    auto& reqs = service::get<service::IRequirements>();
    auto  req  = reqs.resolve(player, request.source, *it);
    if (!request.skipRequirements && !reqs.canAfford(player, req)) {
        return fail("chat.waystones.not_enough_xp", "You do not have enough experience to teleport.");
    }
    int  dim = static_cast<int>(player.getDimensionId());
    auto uid = spawnPortal(dim, *spot, game::opposite(game::playerFacing(player)), request.targetId);
    if (uid.empty()) return noSpace(player);
    if (!request.skipRequirements) reqs.consume(player, req); // paid once the portal stands
    game::consumeAt(player, loc, 1, true);
    auto c = center(core::GlobalPos{dim, spot->x, spot->y, spot->z});
    game::soundAround(dim, "portal.travel", c, 0.25f, 1.4f);
    game::particles(dim, "minecraft:mob_portal", c, 96, 0.6f);
    game::particles(dim, "bedstones:reverse_portal", c, 48, 0.45f);
    log().info("warp portal opened: {} -> {} by {}", uid, request.targetId, player.getRealName());
    return service::WarpResult::success();
}

// ---- module ------------------------------------------------------------------------------------

class WarpPortalModule {
public:
    void install() {
        mTick = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>([](auto&) { tick(); });
        if (!mDbHooked) {
            mDbHooked = true;
            game::db::onWorldSave([] { persistCountdowns(); });
            game::db::onWorldClosed([] {
                gPortals.clear();
                gCooldowns.clear();
            });
        }
    }
    void uninstall() {
        if (mTick) ll::event::EventBus::getInstance().removeListener(mTick);
        mTick.reset();
        persistCountdowns();
    }

private:
    ll::event::ListenerPtr mTick;
    bool                   mDbHooked = false;
};

} // namespace

// ---- public API ----------------------------------------------------------------------------------

std::optional<core::portal::Cell> portalSpotFor(Player& player) {
    auto&  region = player.getDimensionBlockSource();
    auto   feet   = player.getFeetPos();
    double yaw    = static_cast<double>(player.getRotation().y) * 3.14159265358979323846 / 180.0;
    auto   solid  = [&](int x, int y, int z) {
        BlockPos p{x, y, z};
        if (y < region.getMinHeight() || y >= region.getMaxHeight() || !region.hasChunksAt(p, 0, false)) return false;
        return region.getBlock(p).canProvideSupport(1, BlockSupportType::Any);
    };
    auto replaceable = [&](int x, int y, int z) { return game::replaceable(region, BlockPos{x, y, z}); };
    return core::portal::findPortalSpot(feet.x, feet.y, feet.z, -std::sin(yaw), std::cos(yaw), solid, replaceable);
}

std::string spawnPortal(int dim, core::portal::Cell lower, int facing, std::string const& targetId) {
    auto& ws     = game::Waystones::get();
    auto* region = regionFor(dim);
    if (!region || !ws.available()) return {};
    if (!game::placeTwoTall(*region, BlockPos{lower.x, lower.y, lower.z}, portalId(), facing)) return {};
    core::WaystoneRecord rec;
    rec.type      = std::string(core::types::kWarpPortal);
    rec.name      = tr("gui.waystones.waystone_selection.return_to_portal", "Return to Portal");
    rec.pos       = {dim, lower.x, lower.y, lower.z};
    rec.style     = std::string(ids::kWarpPortal);
    rec.facing    = facing & 3;
    rec.transient = true;
    rec.origin    = core::Origin::Player; // no owner, like Waystones' portal records
    rec.visibility               = core::Visibility::ShardOnly;
    rec.extra[kExtraTarget]      = targetId;
    rec.extra[kExtraTicksLeft]   = core::portal::kPortalLifetimeTicks;
    auto* r                      = ws.create(std::move(rec));
    if (!r) {
        game::clearBlock(*region, BlockPos{lower.x, lower.y, lower.z});
        game::clearBlock(*region, BlockPos{lower.x, lower.y + 1, lower.z});
        return {};
    }
    gPortals[r->uid] = PortalState{};
    return r->uid;
}

void removePortal(std::string const& uid, bool effects, std::string_view reason) {
    auto* r = portalRecord(uid);
    if (!r) return;
    auto pos = r->pos;
    if (auto* region = regionFor(pos.dim)) {
        for (int dy = 0; dy <= 1; ++dy) {
            BlockPos p{pos.x, pos.y + dy, pos.z};
            if (game::blockIdAt(*region, p) == portalId()) game::clearBlock(*region, p);
        }
    }
    if (effects) {
        auto c = center(pos);
        game::soundAround(pos.dim, "random.fizz", c, 0.5f, 1.2f);
        game::particles(pos.dim, "bedstones:reverse_portal", c, 32, 0.5f);
    }
    gPortals.erase(uid);
    game::Waystones::get().remove(uid);
    log().info("warp portal closed: {} ({})", uid, reason);
}

void showNoSpace(Player& player) {
    game::chat(player, "§4" + trFor(player, "chat.waystones.warp_portal_no_space", "There is no space to open a Warp Portal here."));
}

void setTicksLeft(std::string const& uid, int ticks) {
    if (auto* r = portalRecord(uid)) stateOf(*r).ticksLeft = ticks;
}

void clearCooldowns() { gCooldowns.clear(); }

} // namespace bs::feature::portal

BS_REGISTER_MODULE(bs::feature::portal::WarpPortalModule);

BS_REGISTER_WARP_INTERCEPTOR(bs::feature::portal::interceptPortalScroll);

// "Return to Portal": the player's last Warp Portal, pinned first (docs/SPEC.md §3.7).
BS_REGISTER_TARGET_PROVIDER(bs::service::TargetProvider{
    .name = "return_portal",
    .list =
        [](Player& player, bs::core::WarpSource const& source, std::vector<bs::core::WarpTarget>& out) {
            using namespace bs;
            if (!feature::portal::listsReturnPortal(source.kind)) return;
            auto& data = game::Waystones::get().playerData(player);
            auto  uid  = data.extra.value(feature::portal::kExtraReturnPortal, std::string{});
            auto* r    = uid.empty() ? nullptr : feature::portal::portalRecord(uid);
            if (!r) return;
            if (std::any_of(out.begin(), out.end(), [&](auto const& t) { return t.id == uid; })) return;
            core::WarpTarget t;
            t.id     = uid;
            t.kind   = std::string(core::types::kWarpPortal);
            t.name   = trFor(player, "gui.waystones.waystone_selection.return_to_portal", "Return to Portal");
            t.pos    = r->pos;
            t.pinned = true;
            t.current = uid == source.fromUid;
            auto& reqs    = service::get<service::IRequirements>();
            t.requirement = reqs.resolve(player, source, t);
            t.canAfford   = reqs.canAfford(player, t.requirement);
            t.extra["color"] = "light_purple";
            out.insert(out.begin(), std::move(t));
        },
    .resolve = [](Player& player, std::string const& id) -> std::optional<bs::service::Destination> {
        using namespace bs;
        auto* r = feature::portal::portalRecord(id);
        if (!r) return std::nullopt;
        // Only the player's own return portal is a valid warp target.
        auto const& data = game::Waystones::get().playerData(player);
        if (data.extra.value(feature::portal::kExtraReturnPortal, std::string{}) != id) return std::nullopt;
        return feature::portal::landingNear(*r);
    },
    .owns     = [](std::string const& id) { return bs::feature::portal::portalRecord(id) != nullptr; },
    .onWarped = {},
});
