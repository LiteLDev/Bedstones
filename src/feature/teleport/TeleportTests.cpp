// Selftests of the teleport pipeline (U7): teleport.*   (test positions dx 700..799)

#include <cmath>
#include <utility>

#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/entity/utilities/leashable_utility/LeashableUtility.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/ActorDefinitionIdentifier.h"
#include "mc/world/actor/ActorFlags.h"
#include "mc/world/actor/Mob.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/provider/SynchedActorDataAccess.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/Spawner.h"
#include "mc/world/level/dimension/DimensionType.h"

#include "feature/teleport/Teleport.h"
#include "game/Blocks.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/Config.h"
#include "net/Companion.h"
#include "net/Screens.h"
#include "selftest/SelfTest.h"
#include "service/Services.h"
#include "service/WarpEvents.h"

namespace bs::feature::teleport {

namespace {

using selftest::Context;
using ll::coro::CoroTask;

// ---- probes: an interceptor and a listener, active only for the target the test names --------
std::string gInterceptTarget;
std::string gListenTarget;
int         gListened = 0;

BS_REGISTER_WARP_INTERCEPTOR([](Player&, service::WarpRequest const& r) -> std::optional<service::WarpResult> {
    if (gInterceptTarget.empty() || r.targetId != gInterceptTarget) return std::nullopt;
    return service::WarpResult::success();
});
BS_REGISTER_WARP_LISTENER([](Player&, service::WarpRequest const& r, service::Destination const&) {
    if (!gListenTarget.empty() && r.targetId == gListenTarget) ++gListened;
});

/// A test IRequirements, swapped in for the duration of a test (restored by the destructor).
struct TestRequirements final : service::IRequirements {
    bool        afford = true;
    std::string refuse;
    int         consumed = 0;
    core::Requirement resolve(Player&, core::WarpSource const&, core::WarpTarget const&) override {
        core::Requirement r;
        r.xpPoints = 5;
        r.refuse   = refuse;
        return r;
    }
    bool canAfford(Player&, core::Requirement const&) override { return afford; }
    void consume(Player&, core::Requirement const&) override { ++consumed; }
};
struct RequirementsSwap {
    TestRequirements*                         test;
    std::unique_ptr<service::IRequirements>   saved;
    RequirementsSwap() {
        auto t = std::make_unique<TestRequirements>();
        test   = t.get();
        saved  = std::exchange(service::detail::slot<service::IRequirements>(), std::move(t));
    }
    ~RequirementsSwap() { service::detail::slot<service::IRequirements>() = std::move(saved); }
};

/// Restores the `teleports` config after a test changed it.
struct ConfigGuard {
    TeleportsConfig saved = config::get<TeleportsConfig>();
    ~ConfigGuard() { config::mut<TeleportsConfig>() = saved; }
};

core::GlobalPos at(core::GlobalPos p, int dx, int dy, int dz) { return {p.dim, p.x + dx, p.y + dy, p.z + dz}; }

/// Clears a 3x3x3 box around `pos` (and remembers the cells for cleanup).
void clearAround(Context& ctx, core::GlobalPos pos) {
    for (int dy = 0; dy < 3; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) ctx.setBlock(at(pos, dx, dy, dz), "minecraft:air");
        }
    }
}

bool waystoneBlockRegistered() { return game::blockWith(ids::id("waystone")) != nullptr; }

/// A waystone record at `pos` (facing `facing`), with its block when the waystone block exists.
core::WaystoneRecord& waystone(Context& ctx, core::GlobalPos pos, int facing, std::string name = "Target") {
    clearAround(ctx, pos);
    core::WaystoneRecord r;
    r.name   = std::move(name);
    r.pos    = pos;
    r.style  = "waystone";
    r.facing = facing;
    r.origin = core::Origin::Player;
    if (waystoneBlockRegistered()) {
        ctx.setBlock(pos, ids::id("waystone"), {{std::string(ids::kStateFacing), facing}, {std::string(ids::kStateUpper), 0}});
        ctx.setBlock(at(pos, 0, 1, 0), ids::id("waystone"), {{std::string(ids::kStateFacing), facing}, {std::string(ids::kStateUpper), 1}});
    }
    return ctx.createWaystone(r);
}

/// The test area `dx` (Context::testPos) at the surface: loads its chunks and finds the first
/// free block above the ground (the world spawn's y is not reliable on BDS).
CoroTask<core::GlobalPos> surface(Context& ctx, int dx, int dz = 0) {
    auto pos = ctx.testPos(dx, dz);
    pos.y    = 0;
    ctx.require(co_await preloadDestination(pos), "test area chunks load");
    auto* region = regionOf(pos.dim);
    ctx.require(region != nullptr, "overworld region");
    pos.y = region->getAboveTopSolidBlock(pos.x, pos.z, false, false);
    co_return pos;
}

/// Moves the test player to `pos` and waits until the chunks around it are loaded.
CoroTask<> goTo(Context& ctx, core::GlobalPos pos) {
    co_await preloadDestination(pos);
    auto& p = ctx.player();
    p.teleport(Vec3{static_cast<float>(pos.x) + 0.5f, static_cast<float>(pos.y), static_cast<float>(pos.z) + 0.5f}, DimensionType(pos.dim));
    for (int i = 0; i < 200; ++i) {
        auto* region = regionOf(pos.dim);
        if (region && region->hasChunksAt(game::blockPos(pos), 2, false) && static_cast<int>(p.getDimensionId()) == pos.dim) break;
        co_await ctx.ticks(1);
    }
    auto* region = regionOf(pos.dim);
    ctx.require(region && region->hasChunksAt(game::blockPos(pos), 2, false), "chunks around the test area loaded");
}

service::WarpResult warpTo(Player& p, std::string const& uid, core::WarpSource source = {}) {
    service::WarpRequest req;
    req.source   = std::move(source);
    req.targetId = uid;
    return service::get<service::ITeleport>().warp(p, req);
}

bool near(Vec3 const& v, float x, float z, float tol = 0.3f) { return std::abs(v.x - x) < tol && std::abs(v.z - z) < tol; }

float yawDiff(float a, float b) {
    float d = std::fmod(std::abs(a - b), 360.f);
    return d > 180.f ? 360.f - d : d;
}

std::string fmt(Vec3 const& v) { return std::to_string(v.x) + " " + std::to_string(v.y) + " " + std::to_string(v.z); }

Mob* spawn(Context& ctx, std::string const& id, Vec3 const& pos) {
    auto& p   = ctx.player();
    auto* mob = p.getLevel().getSpawner().spawnMob(p.getDimensionBlockSource(), ActorDefinitionIdentifier(id), nullptr, pos, false, true, false);
    ctx.require(mob != nullptr, "spawned " + id);
    return mob;
}

void removeMob(ActorUniqueID id) {
    auto lv = ll::service::getLevel();
    if (!lv) return;
    if (auto* a = lv->fetchEntity(id, false)) a->despawn();
}

} // namespace

// ---- landing spot ----------------------------------------------------------------------------------

BS_REGISTER_SELFTEST("teleport.landing_facing", [](Context& ctx) -> CoroTask<> {
    auto pos = co_await surface(ctx, 700);
    co_await goTo(ctx, at(pos, 6, 0, 0));
    auto& p   = ctx.player();
    auto& rec = waystone(ctx, pos, core::teleport::kNorth);
    auto  res = warpTo(p, rec.uid);
    ctx.require(res.ok, "warp succeeded: " + res.error);
    co_await ctx.ticks(2);
    auto feet = p.getFeetPos();
    ctx.check(near(feet, pos.x + 0.5f, pos.z - 0.5f), "lands on the block north of the waystone, got " + fmt(feet));
    ctx.check(std::abs(feet.y - pos.y) < 0.6f, "lands at the waystone's height");
    ctx.check(yawDiff(p.getRotation().y, 180.f) < 2.f, "looks north (yaw 180), got " + std::to_string(p.getRotation().y));
});

BS_REGISTER_SELFTEST("teleport.landing_fallback", [](Context& ctx) -> CoroTask<> {
    auto pos = co_await surface(ctx, 710);
    co_await goTo(ctx, at(pos, 6, 0, 0));
    auto& p   = ctx.player();
    auto& rec = waystone(ctx, pos, core::teleport::kSouth);
    ctx.setBlock(at(pos, 0, 0, 1), "minecraft:stone"); // south: feet blocked
    ctx.setBlock(at(pos, 1, 1, 0), "minecraft:stone"); // east: head blocked
    auto res = warpTo(p, rec.uid);
    ctx.require(res.ok, "warp succeeded: " + res.error);
    co_await ctx.ticks(2);
    auto feet = p.getFeetPos();
    ctx.check(near(feet, pos.x - 0.5f, pos.z + 0.5f), "falls back to the west side, got " + fmt(feet));
    ctx.check(yawDiff(p.getRotation().y, 90.f) < 2.f, "looks west (yaw 90), got " + std::to_string(p.getRotation().y));

    ctx.setBlock(at(pos, -1, 0, 0), "minecraft:stone"); // west
    ctx.setBlock(at(pos, 0, 1, -1), "minecraft:stone"); // north
    auto dest = service::get<service::ITeleport>().resolve(p, rec.uid);
    ctx.require(dest.has_value(), "resolves");
    ctx.check(std::abs(dest->x - (pos.x + 0.5f)) < 0.01f && std::abs(dest->z - (pos.z + 0.5f)) < 0.01f, "all sides blocked: on the waystone itself");
    ctx.check(yawDiff(dest->yaw, 0.f) < 0.01f, "fallback looks along the waystone's facing");
});

// ---- dimensions and chunk loading ---------------------------------------------------------------------

BS_REGISTER_SELFTEST("teleport.cross_dimension", [](Context& ctx) -> CoroTask<> {
    auto& p   = ctx.player();
    auto  ow  = co_await surface(ctx, 720);
    co_await goTo(ctx, ow);
    core::GlobalPos nether{1, ow.x / 8, 64, ow.z / 8};
    bool loaded = co_await preloadDestination(nether);
    ctx.require(loaded, "nether destination chunks load");
    auto* region = regionOf(1);
    ctx.require(region != nullptr, "nether region");
    // An air pocket with an obsidian floor (restored to air afterwards by hand).
    std::vector<core::GlobalPos> cells;
    auto set = [&](core::GlobalPos c, std::string const& id) {
        if (auto const* b = game::blockWith(id)) game::setBlock(*region, game::blockPos(c), *b);
        cells.push_back(c);
    };
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            set(at(nether, dx, -1, dz), "minecraft:obsidian");
            for (int dy = 0; dy < 3; ++dy) set(at(nether, dx, dy, dz), "minecraft:air");
        }
    }
    core::WaystoneRecord r;
    r.name   = "Nether Target";
    r.pos    = nether;
    r.style  = "waystone";
    r.facing = core::teleport::kEast;
    if (waystoneBlockRegistered()) {
        game::placeTwoTall(*region, game::blockPos(nether), ids::id("waystone"), r.facing);
    }
    auto& rec = ctx.createWaystone(r);
    auto  res = warpTo(p, rec.uid);
    ctx.check(res.ok, "cross-dimension warp accepted: " + res.error);
    for (int i = 0; i < 200 && static_cast<int>(p.getDimensionId()) != 1; ++i) co_await ctx.ticks(1);
    co_await ctx.ticks(5);
    ctx.check(static_cast<int>(p.getDimensionId()) == 1, "player is in the nether");
    ctx.check(near(p.getFeetPos(), nether.x + 1.5f, nether.z + 0.5f), "lands east of the nether waystone, got " + fmt(p.getFeetPos()));
    p.teleport(Vec3{ow.x + 0.5f, static_cast<float>(ow.y), ow.z + 0.5f}, DimensionType(0));
    for (auto const& c : cells) game::clearBlock(*region, game::blockPos(c));
    co_await ctx.ticks(10);
});

BS_REGISTER_SELFTEST("teleport.async_load", [](Context& ctx) -> CoroTask<> {
    auto& p = ctx.player();
    auto base = co_await surface(ctx, 730);
    co_await goTo(ctx, base);
    // A record far away in chunks nobody has loaded (no block: only valid while no waystone block exists).
    auto far = at(base, 1, 0, 6000);
    core::WaystoneRecord r;
    r.name  = "Far";
    r.pos   = far;
    r.style = "waystone";
    auto& rec       = ctx.createWaystone(r);
    gListenTarget   = rec.uid;
    gListened       = 0;
    auto res        = warpTo(p, rec.uid);
    ctx.check(res.ok, "far warp accepted (pending): " + res.error);
    ctx.check(gListened == 0, "listeners wait for the actual teleport");
    for (int i = 0; i < 160 && gListened == 0; ++i) co_await ctx.ticks(1);
    gListenTarget.clear();
    if (waystoneBlockRegistered()) {
        ctx.check(gListened == 0, "no waystone block at the far target: refused after loading");
    } else {
        ctx.check(gListened == 1, "teleported once the destination loaded");
        ctx.check(near(p.getFeetPos(), far.x + 0.5f, far.z + 1.5f, 1.f), "arrived at the far target, got " + fmt(p.getFeetPos()));
    }
});

// ---- leashed mobs and pets ------------------------------------------------------------------------------

BS_REGISTER_SELFTEST("teleport.leashed", [](Context& ctx) -> CoroTask<> {
    ConfigGuard guard;
    auto&       p    = ctx.player();
    auto        home = co_await surface(ctx, 740);
    co_await goTo(ctx, home);
    auto& rec    = waystone(ctx, at(home, 5, 0, 20), core::teleport::kNorth);
    auto  uid    = rec.uid;
    auto  netherUid = ctx.createWaystone(core::WaystoneRecord{.name = "Nether", .pos = {1, 0, 64, 0}}).uid;
    auto* cow    = spawn(ctx, "minecraft:cow", Vec3{home.x + 2.5f, static_cast<float>(home.y), home.z + 0.5f});
    auto  cowId  = cow->getOrCreateUniqueID();
    LeashableUtility::leash(*cow, p, false, false);
    co_await ctx.ticks(2);
    ctx.require(findLeashed(p).size() == 1, "the cow is leashed to the player");

    auto& cfg            = config::mut<TeleportsConfig>();
    cfg.transportLeashed = "DISABLED";
    ctx.check(warpTo(p, uid).error == "chat.waystones.cannot_transport_leashed", "DISABLED refuses leashed mobs");
    cfg.transportLeashed = "ENABLED";
    cfg.entityDenyList   = {"minecraft:cow"};
    ctx.check(warpTo(p, uid).error == "chat.waystones.cannot_transport_this_leashed", "denied mob refused");
    cfg.entityDenyList   = {};
    cfg.transportLeashed = "SAME_DIMENSION";
    ctx.check(warpTo(p, netherUid).error == "chat.waystones.cannot_transport_leashed_dimensional", "SAME_DIMENSION refuses other worlds");

    cfg.transportLeashed = "ENABLED";
    auto res             = warpTo(p, uid);
    ctx.require(res.ok, "leashed warp succeeded: " + res.error);
    co_await ctx.ticks(3);
    auto lv = ll::service::getLevel();
    cow     = lv ? static_cast<Mob*>(lv->fetchEntity(cowId, false)) : nullptr;
    ctx.require(cow != nullptr, "cow still exists");
    ctx.check(cow->getFeetPos().distanceTo(p.getFeetPos()) < 3.f, "the cow came along, at " + fmt(cow->getFeetPos()));
    ctx.check(cow->isLeashed() && cow->getLeashHolder() == p.getOrCreateUniqueID(), "still leashed to the player");
    LeashableUtility::unleash(*cow, LeashableUtility::UnleashReason::Removed);
    removeMob(cowId);
});

BS_REGISTER_SELFTEST("teleport.pets", [](Context& ctx) -> CoroTask<> {
    ConfigGuard guard;
    auto&       p    = ctx.player();
    auto        home = co_await surface(ctx, 750);
    co_await goTo(ctx, home);
    auto& rec   = waystone(ctx, at(home, 5, 0, 20), core::teleport::kNorth);
    auto  uid   = rec.uid;
    auto* wolf  = spawn(ctx, "minecraft:wolf", Vec3{home.x + 2.5f, static_cast<float>(home.y), home.z + 0.5f});
    auto  id    = wolf->getOrCreateUniqueID();
    wolf->setOwner(p.getOrCreateUniqueID());
    SynchedActorDataAccess::setActorFlag(wolf->getEntityContext(), ActorFlags::Tamed, true);
    ctx.require(wolf->isTame(), "the wolf is tamed");
    ctx.check(findPets(p).size() == 1, "the tamed wolf is a pet of the player");

    SynchedActorDataAccess::setActorFlag(wolf->getEntityContext(), ActorFlags::Sitting, true);
    ctx.check(findPets(p).empty(), "sitting pets stay");
    SynchedActorDataAccess::setActorFlag(wolf->getEntityContext(), ActorFlags::Sitting, false);
    auto& cfg          = config::mut<TeleportsConfig>();
    cfg.entityDenyList = {"minecraft:wolf"};
    ctx.check(findPets(p).empty(), "denied pets stay");
    cfg.entityDenyList = {};

    cfg.transportPets = "DISABLED";
    auto before       = wolf->getFeetPos();
    ctx.require(warpTo(p, uid).ok, "warp with pets disabled");
    ctx.check(wolf->getFeetPos().distanceTo(before) < 0.5f, "DISABLED: the wolf stays");
    co_await goTo(ctx, home);
    wolf = static_cast<Mob*>(p.getLevel().fetchEntity(id, false));
    ctx.require(wolf != nullptr, "wolf still exists");
    wolf->teleport(Vec3{home.x + 2.5f, static_cast<float>(home.y), home.z + 0.5f}, DimensionType(0));
    co_await ctx.ticks(2);

    cfg.transportPets = "ENABLED";
    ctx.require(warpTo(p, uid).ok, "warp with pets enabled");
    co_await ctx.ticks(2);
    wolf = static_cast<Mob*>(p.getLevel().fetchEntity(id, false));
    ctx.require(wolf != nullptr, "wolf still exists");
    ctx.check(wolf->getFeetPos().distanceTo(p.getFeetPos()) < 3.f, "ENABLED: the wolf came along");
    removeMob(id);
});

// ---- validation ----------------------------------------------------------------------------------------------

BS_REGISTER_SELFTEST("teleport.item_not_held", [](Context& ctx) -> CoroTask<> {
    auto& p = ctx.player();
    auto base = co_await surface(ctx, 760);
    co_await goTo(ctx, base);
    auto&            rec = waystone(ctx, at(base, 5, 0, 0), core::teleport::kNorth);
    core::WarpSource src;
    src.kind   = core::WarpSourceKind::WarpScroll;
    src.itemId = "minecraft:stick";
    src.hand   = 0;
    game::setItemAt(p, game::handSlot(p, 0), ItemStack::EMPTY_ITEM());
    auto res = warpTo(p, rec.uid, src);
    ctx.check(!res.ok && res.error == "chat.waystones.source_item_missing", "refused without the item: " + res.error);
    game::setItemAt(p, game::handSlot(p, 0), game::makeItem("minecraft:stick"));
    res = warpTo(p, rec.uid, src);
    ctx.check(res.ok, "warps while holding the item: " + res.error);
    game::setItemAt(p, game::handSlot(p, 0), ItemStack::EMPTY_ITEM());
});

BS_REGISTER_SELFTEST("teleport.source_too_far", [](Context& ctx) -> CoroTask<> {
    auto& p      = ctx.player();
    auto  source = co_await surface(ctx, 770);
    auto& from   = waystone(ctx, source, core::teleport::kSouth, "Source");
    auto& to     = waystone(ctx, at(source, 5, 0, 0), core::teleport::kNorth);
    core::WarpSource src;
    src.kind    = core::WarpSourceKind::Waystone;
    src.fromUid = from.uid;
    src.fromPos = from.pos;
    co_await goTo(ctx, at(source, 0, 0, 20));
    auto res = warpTo(p, to.uid, src);
    ctx.check(!res.ok && res.error == "chat.waystones.source_waystone_out_of_range", "refused 20 blocks away: " + res.error);
    co_await goTo(ctx, at(source, 0, 0, 2));
    res = warpTo(p, to.uid, src);
    ctx.check(res.ok, "warps next to the source: " + res.error);
});

BS_REGISTER_SELFTEST("teleport.missing_target", [](Context& ctx) -> CoroTask<> {
    auto& p = ctx.player();
    auto base = co_await surface(ctx, 780);
    co_await goTo(ctx, base);
    auto& rec = waystone(ctx, at(base, 4, 0, 0), core::teleport::kNorth);
    auto  uid = rec.uid;
    game::Waystones::get().remove(uid);
    auto res = warpTo(p, uid);
    ctx.check(!res.ok && res.error == "chat.waystones.waystone_missing", "removed record: " + res.error);
    if (waystoneBlockRegistered()) {
        core::WaystoneRecord r;
        r.pos     = at(base, 6, 0, 0);
        r.style   = "waystone";
        auto& bad = ctx.createWaystone(r); // record without its block
        res       = warpTo(p, bad.uid);
        ctx.check(!res.ok && res.error == "chat.waystones.waystone_missing", "record without a block: " + res.error);
    }
});

BS_REGISTER_SELFTEST("teleport.requirements", [](Context& ctx) -> CoroTask<> {
    auto& p = ctx.player();
    auto base = co_await surface(ctx, 788);
    co_await goTo(ctx, base);
    auto&            rec = waystone(ctx, at(base, 2, 0, 0), core::teleport::kNorth);
    RequirementsSwap swap;
    swap.test->afford = false;
    auto res          = warpTo(p, rec.uid);
    ctx.check(res.error == "chat.waystones.not_enough_xp", "unaffordable: not enough experience, got " + res.error);
    swap.test->refuse = "$chat.waystones.test_refusal";
    res               = warpTo(p, rec.uid);
    ctx.check(res.error == "chat.waystones.test_refusal", "the requirement's refuse message wins, got " + res.error);
    ctx.check(swap.test->consumed == 0, "nothing consumed when refused");
    swap.test->refuse = "";
    swap.test->afford = true;
    res               = warpTo(p, rec.uid);
    ctx.check(res.ok, "affordable: warps");
    ctx.check(swap.test->consumed == 1, "requirements consumed once");
});

// ---- events ----------------------------------------------------------------------------------------------------

BS_REGISTER_SELFTEST("teleport.interceptor", [](Context& ctx) -> CoroTask<> {
    auto& p = ctx.player();
    auto base = co_await surface(ctx, 792);
    co_await goTo(ctx, base);
    auto&            rec = waystone(ctx, at(base, 2, 0, 0), core::teleport::kNorth);
    RequirementsSwap swap;
    gInterceptTarget = rec.uid;
    gListenTarget    = rec.uid;
    gListened        = 0;
    auto before      = p.getFeetPos();
    auto res         = warpTo(p, rec.uid);
    gInterceptTarget.clear();
    co_await ctx.ticks(2);
    ctx.check(res.ok, "the interceptor's result is returned");
    ctx.check(p.getFeetPos().distanceTo(before) < 0.5f, "intercepted: the player did not move");
    ctx.check(swap.test->consumed == 0, "intercepted: nothing paid");
    ctx.check(gListened == 0, "intercepted: no warp listeners");
    gListenTarget.clear();
});

BS_REGISTER_SELFTEST("teleport.listeners", [](Context& ctx) -> CoroTask<> {
    auto& p = ctx.player();
    auto base = co_await surface(ctx, 796);
    co_await goTo(ctx, base);
    auto& rec     = waystone(ctx, at(base, 2, 0, 0), core::teleport::kNorth);
    gListenTarget = rec.uid;
    gListened     = 0;
    ctx.check(warpTo(p, rec.uid).ok, "warp ok");
    ctx.check(gListened == 1, "listeners run once after a successful warp");
    {
        RequirementsSwap swap;
        swap.test->afford = false;
        ctx.check(!warpTo(p, rec.uid).ok, "refused warp");
    }
    auto uid = rec.uid;
    game::Waystones::get().remove(uid);
    gListenTarget = uid;
    ctx.check(!warpTo(p, uid).ok, "missing target refused");
    ctx.check(gListened == 1, "listeners never run for failed warps");
    gListenTarget.clear();
});

// ---- menus close when the player walks away ---------------------------------------------------------------------

BS_REGISTER_SELFTEST("teleport.menu_close", [](Context& ctx) -> CoroTask<> {
    auto& p      = ctx.player();
    auto  source = co_await surface(ctx, 799);
    auto& from   = waystone(ctx, source, core::teleport::kSouth, "Menu Source");
    co_await goTo(ctx, at(source, 0, 0, 2));
    net::setTestSink([](Player&, std::string_view, nlohmann::json const&) {});
    net::setCompanion(p, net::CompanionInfo{"test", {"native_screen"}});
    core::WarpSource src;
    src.kind    = core::WarpSourceKind::Waystone;
    src.fromUid = from.uid;
    src.fromPos = from.pos;
    auto session = net::screens::open(p, "teleport_test", nlohmann::json::object(), {{"source", src.toJson()}});
    ctx.require(session != 0, "screen opened");
    co_await ctx.ticks(10);
    ctx.check(net::screens::current(p) != nullptr, "stays open next to the source");
    co_await goTo(ctx, at(source, 0, 0, 20));
    co_await ctx.ticks(10);
    ctx.check(net::screens::current(p) == nullptr, "closed after moving 20 blocks away");
    net::screens::close(p);
    net::setCompanion(p, std::nullopt);
    net::setTestSink(nullptr);
});

} // namespace bs::feature::teleport
