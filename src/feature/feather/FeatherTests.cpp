// Selftests of the Twinbound Feather (`/waystones selftest "feather.*"`). Test area dx 600..699.

#include <algorithm>
#include <cmath>

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/server/SimulatedPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/dimension/DimensionType.h"

#include "feature/feather/Feather.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "selftest/SelfTest.h"
#include "service/Services.h"

namespace bs::feature::feather {

namespace {

using selftest::Context;

ItemStack feather() { return game::makeItem(ids::id(ids::kTwinboundFeather)); }

SimulatedPlayer& sim(Player& p) { return static_cast<SimulatedPlayer&>(p); }

Vec3 feetAt(core::GlobalPos const& pos, float dz = 0) {
    return Vec3{static_cast<float>(pos.x) + 0.5f, static_cast<float>(pos.y), static_cast<float>(pos.z) + 0.5f + dz};
}

/// Two players with a feather in the selected slot, standing 1.5 blocks apart facing each other.
ll::coro::CoroTask<> faceToFace(Context& ctx, Player& a, Player& b, int dx) {
    auto pos = ctx.testPos(dx);
    ctx.give(a, feather(), 0);
    ctx.give(b, feather(), 0);
    a.teleport(feetAt(pos), DimensionType(0), Vec2{0.f, 0.f});     // facing south (+z)
    b.teleport(feetAt(pos, 1.5f), DimensionType(0), Vec2{0.f, 180.f}); // facing north
    co_await ctx.ticks(3);
    sim(a).simulateLookAt(b, sim::LookDuration::Continuous);
    sim(b).simulateLookAt(a, sim::LookDuration::Continuous);
    co_await ctx.ticks(2);
}

bool listed(Player& viewer, Player& partner, core::WarpSourceKind kind, core::WarpTarget* out = nullptr) {
    core::WarpSource src;
    src.kind  = kind;
    auto list = service::get<service::ITargets>().list(viewer, src);
    auto id   = targetIdOf(partner);
    auto it   = std::find_if(list.begin(), list.end(), [&](auto const& t) { return t.id == id; });
    if (it == list.end()) return false;
    if (out) *out = *it;
    return true;
}

ItemStack const& slot0(Player& p) { return game::itemAt(p, {game::ItemLocation::Kind::Inventory, 0}); }

} // namespace

BS_REGISTER_SELFTEST("feather.ritual", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& a = ctx.player();
    auto& b = ctx.spawnPlayer();
    co_await faceToFace(ctx, a, b, 600);
    simulateUse(a, true);
    simulateUse(b, true);
    ctx.check(isUsingFeather(a) && isUsingFeather(b), "both players hold use with a feather");
    co_await ctx.ticks(10);
    ctx.check(!arePartners(a, b), "not linked before 20 ticks");
    co_await ctx.ticks(20);
    auto fa = infoOf(slot0(a));
    auto fb = infoOf(slot0(b));
    ctx.check(!fa.id.empty() && !fb.id.empty() && fa.id != fb.id, "each feather got its own id");
    ctx.check(core::feather::areLinked(fa, fb), "feathers are linked to each other");
    ctx.check(arePartners(a, b) && arePartners(b, a), "players are partners");
    ctx.check(!isUsingFeather(a) && !isUsingFeather(b), "use stops after linking");
    auto const& extra = game::Waystones::get().playerData(a).extra;
    ctx.check(extra.contains("feather") && extra["feather"]["links"].contains(game::Waystones::playerKey(b)), "pairing recorded in player data");
    ctx.check(!slot0(a).getCustomLore().empty(), "lore shows the partner");
});

BS_REGISTER_SELFTEST("feather.ritual_needs_eye_contact", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& a = ctx.player();
    auto& b = ctx.spawnPlayer();
    co_await faceToFace(ctx, a, b, 610);
    // b turns away: only a looks at b.
    sim(b).simulateLookAt(feetAt(ctx.testPos(610), 6.f), sim::LookDuration::Continuous);
    simulateUse(a, true);
    simulateUse(b, true);
    co_await ctx.ticks(30);
    ctx.check(!arePartners(a, b), "no link without looking at each other");
    // Only one of them using: no link either.
    sim(b).simulateLookAt(a, sim::LookDuration::Continuous);
    simulateUse(b, false);
    co_await ctx.ticks(30);
    ctx.check(!arePartners(a, b), "no link when the partner does not use a feather");
    // Too far apart.
    simulateUse(b, true);
    b.teleport(feetAt(ctx.testPos(610), 3.5f), DimensionType(0), Vec2{0.f, 180.f});
    co_await ctx.ticks(30);
    ctx.check(!arePartners(a, b), "no link beyond 2 blocks");
    simulateUse(a, false);
    simulateUse(b, false);
});

BS_REGISTER_SELFTEST("feather.ritual_blocked_by_player", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& a = ctx.player();
    auto& b = ctx.spawnPlayer();
    auto& c = ctx.spawnPlayer();
    co_await faceToFace(ctx, a, b, 620);
    // c (no feather) stands between them: the look rays hit c first.
    b.teleport(feetAt(ctx.testPos(620), 1.9f), DimensionType(0), Vec2{0.f, 180.f});
    c.teleport(feetAt(ctx.testPos(620), 0.95f), DimensionType(0), Vec2{0.f, 0.f});
    co_await ctx.ticks(3);
    sim(a).simulateLookAt(b, sim::LookDuration::Continuous);
    sim(b).simulateLookAt(a, sim::LookDuration::Continuous);
    simulateUse(a, true);
    simulateUse(b, true);
    co_await ctx.ticks(30);
    ctx.check(!arePartners(a, b), "a player in between blocks the ritual");
    simulateUse(a, false);
    simulateUse(b, false);
});

BS_REGISTER_SELFTEST("feather.targets", [](Context& ctx) {
    auto& a = ctx.player();
    auto& b = ctx.spawnPlayer();
    ctx.give(a, feather(), 0);
    ctx.give(b, feather(), 5);
    ctx.require(link(a, b).result == LinkResult::Success, "link helper links");
    core::WarpTarget t;
    ctx.check(listed(a, b, core::WarpSourceKind::Waystone, &t), "partner listed in waystone menus");
    ctx.check(t.kind == "player" && t.name == b.getRealName(), "entry is the partner's name, kind player");
    ctx.check(std::find(t.groups.begin(), t.groups.end(), service::groups::kPlayers) != t.groups.end(), "entry is in the Players group");
    ctx.check(listed(b, a, core::WarpSourceKind::WarpScroll), "listed both ways (warp scroll)");
    ctx.check(listed(a, b, core::WarpSourceKind::WarpStone), "warp stone menus list partners");
    ctx.check(listed(a, b, core::WarpSourceKind::InventoryButton), "inventory button menus list partners");
    ctx.check(!listed(a, b, core::WarpSourceKind::Sharestone), "not in sharestone menus");
    ctx.check(!listed(a, b, core::WarpSourceKind::Portstone), "not in portstone menus");
    ctx.check(!listed(a, b, core::WarpSourceKind::WarpPlate), "not for warp plates");
    ctx.check(!listed(a, b, core::WarpSourceKind::PortalScroll), "not in portal scroll menus");
    // Offhand counts as carrying.
    auto off = slot0(a);
    game::setItemAt(a, {game::ItemLocation::Kind::Inventory, 0}, ItemStack::EMPTY_ITEM());
    game::setItemAt(a, {game::ItemLocation::Kind::Offhand, 0}, off);
    ctx.check(listed(a, b, core::WarpSourceKind::Waystone), "a feather in the offhand counts");
});

BS_REGISTER_SELFTEST("feather.warp", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& a = ctx.player();
    auto& b = ctx.spawnPlayer();
    ctx.give(a, feather(), 0);
    ctx.give(b, feather(), 0);
    ctx.require(link(a, b).result == LinkResult::Success, "linked");
    a.teleport(feetAt(ctx.testPos(630)), DimensionType(0), Vec2{0.f, 0.f});
    b.teleport(feetAt(ctx.testPos(650, 7)), DimensionType(0), Vec2{0.f, 90.f});
    co_await ctx.ticks(3);
    auto target = b.getFeetPos();
    service::WarpRequest req;
    req.source.kind = core::WarpSourceKind::WarpScroll;
    req.targetId    = targetIdOf(b);
    auto res        = service::get<service::ITeleport>().warp(a, req);
    ctx.check(res.ok, "warp to the partner succeeded: " + res.error);
    co_await ctx.ticks(3);
    auto pos = a.getFeetPos();
    ctx.check(std::abs(pos.x - target.x) < 1.f && std::abs(pos.z - target.z) < 1.f, "landed at the partner");
});

BS_REGISTER_SELFTEST("feather.unlisted", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& a = ctx.player();
    auto& b = ctx.spawnPlayer();
    ctx.give(a, feather(), 0);
    ctx.give(b, feather(), 0);
    ctx.check(!listed(a, b, core::WarpSourceKind::Waystone), "unlinked feathers are not listed");
    ctx.require(link(a, b).result == LinkResult::Success, "linked");
    // The partner no longer carries the feather.
    auto kept = slot0(b);
    game::setItemAt(b, {game::ItemLocation::Kind::Inventory, 0}, ItemStack::EMPTY_ITEM());
    ctx.check(!listed(a, b, core::WarpSourceKind::Waystone), "partner without the feather is not listed");
    ctx.check(!service::get<service::ITeleport>().resolve(a, targetIdOf(b)), "cannot warp to a partner without the feather");
    game::setItemAt(b, {game::ItemLocation::Kind::Inventory, 0}, kept);
    ctx.check(listed(a, b, core::WarpSourceKind::Waystone), "listed again once carried");
    // An offline partner.
    auto c = SimulatedPlayer::create("bs_feather_offline", a.getFeetPos(), DimensionType(0));
    ctx.require(bool(c), "spawned a third player");
    ctx.give(*c, feather(), 0);
    ctx.give(a, feather(), 1);
    a.setSelectedSlot(1);
    ctx.require(link(a, *c).result == LinkResult::Success, "linked with the third player");
    ctx.check(listed(a, *c, core::WarpSourceKind::Waystone), "online partner listed");
    auto cid = targetIdOf(*c);
    c->simulateDisconnect();
    co_await ctx.ticks(3);
    core::WarpSource src;
    src.kind  = core::WarpSourceKind::Waystone;
    auto list = service::get<service::ITargets>().list(a, src);
    ctx.check(std::none_of(list.begin(), list.end(), [&](auto const& t) { return t.id == cid; }), "offline partner is not listed");
    ctx.check(!service::get<service::ITeleport>().resolve(a, cid), "offline partner cannot be resolved");
    a.setSelectedSlot(0);
});

BS_REGISTER_SELFTEST("feather.link_helper", [](Context& ctx) {
    auto& a = ctx.player();
    auto& b = ctx.spawnPlayer();
    ctx.check(link(a, a).result == LinkResult::SamePlayer, "same player refused");
    auto r = link(a, b);
    ctx.check(r.result == LinkResult::Success, "players without feathers get linked feathers");
    ctx.check(!carriedFeathers(a).empty() && !carriedFeathers(b).empty(), "feathers were given");
    ctx.check(link(a, b).result == LinkResult::AlreadyLinked, "already linked");
    // A second, unlinked feather in hand does not create another link.
    ctx.give(a, feather(), 8);
    a.setSelectedSlot(8);
    ctx.check(link(a, b).result == LinkResult::AlreadyLinked, "already partners with another feather in hand");
    ctx.check(!infoOf(game::itemAt(a, {game::ItemLocation::Kind::Inventory, 8})).id.size(), "the second feather stays unlinked");
    a.setSelectedSlot(0);
    auto& d = ctx.spawnPlayer();
    auto  noFeather = link(a, d, false);
    ctx.check(noFeather.result == LinkResult::NoFeather && noFeather.player == d.getRealName(), "no feather and no give");
    auto& c = ctx.spawnPlayer();
    for (int i = 0; i < 36; ++i) ctx.give(c, game::makeItem("minecraft:dirt"), i);
    auto full = link(a, c);
    ctx.check(full.result == LinkResult::NoSpace && full.player == c.getRealName(), "no space for a feather");
});

} // namespace bs::feature::feather
