// Selftests of the Portal Scroll and the Warp Portal (`/waystones selftest "portal.*"`).

#include <cmath>

#include "mc/server/SimulatedPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/GameType.h"

#include "feature/portal/Portal.h"
#include "feature/portal/TestArena.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "service/Services.h"

namespace {

using bs::selftest::Context;
namespace portal = bs::feature::portal;
namespace arena  = bs::feature::portal::test;

std::string const kPortal = "bedstones:warp_portal";
std::string const kScroll = "bedstones:portal_scroll";

std::string blockAt(bs::core::GlobalPos p) {
    auto* region = arena::overworld();
    return region ? bs::game::blockIdAt(*region, bs::game::blockPos(p)) : std::string{};
}

int countOf(Player& player, std::string const& id) {
    int  n   = 0;
    auto& inv = player.getInventory();
    for (int i = 0; i < inv.getContainerSize(); ++i) {
        auto const& s = inv.getItem(i);
        if (bs::game::idOf(s) == id) n += s.mCount;
    }
    return n;
}

bs::core::WaystoneRecord* portalAt(bs::core::GlobalPos p) {
    auto* r = bs::game::Waystones::get().findAt(p);
    return r && r->type == bs::core::types::kWarpPortal ? r : nullptr;
}

bs::service::WarpRequest scrollWarp(std::string target) {
    bs::service::WarpRequest req;
    req.source.kind   = bs::core::WarpSourceKind::PortalScroll;
    req.source.itemId = kScroll;
    req.source.hand   = 0;
    req.targetId      = std::move(target);
    return req;
}

} // namespace

// The interceptor opens a portal 3 blocks ahead instead of teleporting and uses up one scroll.
BS_REGISTER_SELFTEST("portal.scroll_opens_portal", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p    = ctx.player();
    auto  feet = arena::arena(ctx, 500);
    ctx.require(co_await arena::place(ctx, p, feet, arena::Floor{-1, 6, -2, 2}), "test area loaded");
    p.setPlayerGameType(GameType::Survival);
    p.addLevels(30);
    auto& target = arena::target(ctx, p, {0, feet.x + 20, feet.y, feet.z}, "Portal Target");
    auto  tuid   = target.uid;
    ctx.give(p, bs::game::makeItem(kScroll, 2), p.getSelectedItemSlot());
    auto before = p.getPosition();
    auto res    = bs::service::get<bs::service::ITeleport>().warp(p, scrollWarp(tuid));
    ctx.require(res.ok, "portal scroll warp succeeded: " + res.error);
    bs::core::GlobalPos lower{0, feet.x + 3, feet.y, feet.z};
    auto*               r = portalAt(lower);
    ctx.require(r != nullptr, "portal record 3 blocks ahead");
    ctx.cleanupWaystone(r->uid);
    ctx.cleanupBlock(lower);
    ctx.cleanupBlock({0, lower.x, lower.y + 1, lower.z});
    ctx.check(r->transient, "portal record is transient");
    ctx.check(r->name == "Return to Portal", "portal record name");
    ctx.check(r->extra.value(portal::kExtraTarget, std::string{}) == tuid, "portal target stored");
    ctx.check(r->facing == 1, "portal faces the player (west)");
    ctx.check(blockAt(lower) == kPortal && blockAt({0, lower.x, lower.y + 1, lower.z}) == kPortal, "two-tall portal blocks");
    ctx.check(countOf(p, kScroll) == 1, "one portal scroll used up");
    ctx.check(p.getPosition().distanceTo(before) < 1.f, "the player is not teleported");
});

// Without room ahead the scroll fails with the Waystones message and is not used up.
BS_REGISTER_SELFTEST("portal.no_space", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p    = ctx.player();
    auto  feet = arena::arena(ctx, 520);
    feet.y += 40; // in the air: nothing solid below
    ctx.require(co_await arena::place(ctx, p, feet), "test area loaded");
    p.setPlayerGameType(GameType::Survival);
    p.addLevels(30);
    auto& target = arena::target(ctx, p, {0, feet.x + 20, feet.y - 40, feet.z}, "Unreachable");
    ctx.give(p, bs::game::makeItem(kScroll, 1), p.getSelectedItemSlot());
    ctx.check(!portal::portalSpotFor(p).has_value(), "no portal spot in the air");
    ctx.check(!portal::onScrollCharged(p, 0), "charging reports no space");
    auto res = bs::service::get<bs::service::ITeleport>().warp(p, scrollWarp(target.uid));
    ctx.check(!res.ok && res.error == "chat.waystones.warp_portal_no_space", "warp fails with warp_portal_no_space");
    ctx.check(countOf(p, kScroll) == 1, "scroll kept");
});

// Walking into the portal warps to its target and remembers it as the return portal, which is
// then listed pinned for warp scrolls (not for portal scrolls).
BS_REGISTER_SELFTEST("portal.enter_teleports", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p    = ctx.player();
    auto  feet = arena::arena(ctx, 540);
    ctx.require(co_await arena::place(ctx, p, feet, arena::Floor{-1, 6, -2, 2}), "test area loaded");
    p.setPlayerGameType(GameType::Survival);
    p.addLevels(30);
    auto& target = arena::target(ctx, p, {0, feet.x + 15, feet.y, feet.z}, "Portal Exit");
    auto  tuid   = target.uid;
    portal::clearCooldowns();
    ctx.require(bs::game::blockWith(kPortal) != nullptr, "warp portal block registered");
    auto uid = portal::spawnPortal(0, {feet.x + 3, feet.y, feet.z}, 1, tuid, &p);
    ctx.require(!uid.empty(), "portal spawned");
    ctx.cleanupWaystone(uid);
    ctx.cleanupBlock({0, feet.x + 3, feet.y, feet.z});
    ctx.cleanupBlock({0, feet.x + 3, feet.y + 1, feet.z});
    p.teleport(Vec3{static_cast<float>(feet.x + 3) + 0.5f, static_cast<float>(feet.y), static_cast<float>(feet.z) + 0.5f}, DimensionType(0));
    co_await ctx.ticks(5);
    auto pos = p.getPosition();
    ctx.check(std::abs(pos.x - (static_cast<float>(feet.x + 15) + 0.5f)) < 1.5f, "warped to the portal's target (x)");
    auto& data = bs::game::Waystones::get().playerData(p);
    ctx.check(data.extra.value(portal::kExtraReturnPortal, std::string{}) == uid, "returnPortal remembered");
    bs::core::WarpSource scroll;
    scroll.kind = bs::core::WarpSourceKind::WarpScroll;
    auto list   = bs::service::get<bs::service::ITargets>().list(p, scroll);
    bool listed = false;
    for (auto const& t : list) {
        if (t.id == uid) listed = t.pinned && t.kind == "warp_portal" && t.name == "Return to Portal";
    }
    ctx.check(listed, "return portal listed pinned for a warp scroll");
    bs::core::WarpSource portalScroll;
    portalScroll.kind = bs::core::WarpSourceKind::PortalScroll;
    bool listedForPortal = false;
    for (auto const& t : bs::service::get<bs::service::ITargets>().list(p, portalScroll)) listedForPortal |= t.id == uid;
    ctx.check(!listedForPortal, "return portal not listed for a portal scroll");
    auto dest = bs::service::get<bs::service::ITeleport>().resolve(p, uid);
    ctx.check(dest && std::abs(dest->x - (static_cast<float>(feet.x + 2) + 0.5f)) < 0.01f, "return portal resolves in front of it");
});

// A portal closes after its lifetime with its blocks.
BS_REGISTER_SELFTEST("portal.expiry", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p    = ctx.player();
    auto  feet = arena::arena(ctx, 560);
    ctx.require(co_await arena::place(ctx, p, feet, arena::Floor{-1, 6, -2, 2}), "test area loaded");
    auto& target = arena::target(ctx, p, {0, feet.x + 15, feet.y, feet.z}, "Expiry Target");
    bs::core::GlobalPos lower{0, feet.x + 3, feet.y, feet.z};
    auto                uid = portal::spawnPortal(0, {lower.x, lower.y, lower.z}, 1, target.uid, &p);
    ctx.require(!uid.empty(), "portal spawned");
    ctx.cleanupWaystone(uid);
    ctx.cleanupBlock(lower);
    ctx.cleanupBlock({0, lower.x, lower.y + 1, lower.z});
    co_await ctx.ticks(3);
    ctx.check(bs::game::Waystones::get().find(uid) != nullptr, "portal still open");
    portal::setTicksLeft(uid, 2);
    co_await ctx.ticks(5);
    ctx.check(bs::game::Waystones::get().find(uid) == nullptr, "portal record removed after expiry");
    ctx.check(blockAt(lower) != kPortal && blockAt({0, lower.x, lower.y + 1, lower.z}) != kPortal, "portal blocks removed");
});

// A portal whose target is gone closes when entered and does not move the player.
BS_REGISTER_SELFTEST("portal.target_destroyed", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p    = ctx.player();
    auto  feet = arena::arena(ctx, 580);
    ctx.require(co_await arena::place(ctx, p, feet, arena::Floor{-1, 6, -2, 2}), "test area loaded");
    portal::clearCooldowns();
    bs::core::GlobalPos lower{0, feet.x + 3, feet.y, feet.z};
    auto                uid = portal::spawnPortal(0, {lower.x, lower.y, lower.z}, 1, "00000000-0000-4000-8000-000000000000", &p);
    ctx.require(!uid.empty(), "portal spawned");
    ctx.cleanupWaystone(uid);
    ctx.cleanupBlock(lower);
    ctx.cleanupBlock({0, lower.x, lower.y + 1, lower.z});
    Vec3 inside{static_cast<float>(lower.x) + 0.5f, static_cast<float>(lower.y), static_cast<float>(lower.z) + 0.5f};
    p.teleport(inside, DimensionType(0));
    co_await ctx.ticks(4);
    ctx.check(bs::game::Waystones::get().find(uid) == nullptr, "portal closed");
    ctx.check(blockAt(lower) != kPortal, "portal block removed");
    ctx.check(p.getPosition().distanceTo(inside) < 2.f, "player stays");
});

// Holding use with the scroll for 32 ticks charges it (opens the selection menu).
BS_REGISTER_SELFTEST("portal.charge", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p    = static_cast<SimulatedPlayer&>(ctx.player());
    auto  feet = arena::arena(ctx, 590);
    ctx.require(co_await arena::place(ctx, p, feet, arena::Floor{-1, 6, -2, 2}), "test area loaded");
    p.setPlayerGameType(GameType::Survival);
    ctx.give(p, bs::game::makeItem(kScroll, 1), p.getSelectedItemSlot());
    int before = portal::chargeCount();
    ctx.require(p.simulateUseItem(), "use started");
    co_await ctx.ticks(10);
    ctx.check(portal::chargeCount() == before, "not charged after 10 ticks");
    co_await ctx.ticks(30);
    ctx.check(portal::chargeCount() == before + 1, "charged after 32 ticks");
    p.simulateStopUsingItem();
});
