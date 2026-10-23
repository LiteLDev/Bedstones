// Selftests of U4 (scrolls.*): binding, bound / return scroll warps, consumption, durability,
// creative, the selection seam call and the charge loop. Test positions use dx 400..499.

#include <cmath>
#include <memory>

#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/GameType.h"

#include "feature/scrolls/Scrolls.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/Config.h"
#include "selftest/SelfTest.h"
#include "service/Services.h"

namespace bs::feature::scrolls {

namespace {

using core::scrolls::ScrollItem;
using selftest::Context;

core::WaystoneRecord record(Context& ctx, int dx, std::string name, std::string type = std::string(core::types::kWaystone)) {
    core::WaystoneRecord r;
    r.name   = std::move(name);
    r.type   = std::move(type);
    r.pos    = ctx.testPos(dx);
    r.style  = "waystone";
    r.origin = core::Origin::Player;
    return r;
}

Player& survivalPlayer(Context& ctx) {
    auto& p = ctx.player();
    p.setPlayerGameType(GameType::Survival);
    return p;
}

ItemStack item(std::string_view path, int count = 1) { return game::makeItem(ids::id(path), count); }

ItemStack const& hand(Player& p) { return game::itemAt(p, game::handSlot(p, 0)); }

void giveHand(Context& ctx, Player& p, ItemStack const& s) { ctx.give(p, s, p.getSelectedItemSlot()); }

int countOf(Player& p, std::string_view path) {
    int  n   = 0;
    auto id  = ids::id(path);
    auto& inv = p.getInventory();
    for (int i = 0; i < inv.getContainerSize(); ++i) {
        if (game::idOf(inv.getItem(i)) == id) n += inv.getItem(i).mCount;
    }
    return n;
}

bool near(Player& p, core::WaystoneRecord const& r) {
    auto pos = p.getFeetPos();
    return std::abs(pos.x - (static_cast<float>(r.pos.x) + 0.5f)) < 2.f && std::abs(pos.z - (static_cast<float>(r.pos.z) + 0.5f)) < 2.f;
}

service::WarpRequest itemWarp(core::WarpSourceKind kind, std::string_view path, std::string target) {
    service::WarpRequest req;
    req.source.kind   = kind;
    req.source.itemId = ids::id(path);
    req.targetId      = std::move(target);
    return req;
}

/// Temporarily replaces the selection seam with a probe that records `open` calls.
class SelectionProbe {
public:
    struct Probe final : service::ISelection {
        int                      opened = 0;
        core::WarpSource         last;
        void open(Player&, core::WarpSource const& s) override { ++opened, last = s; }
        void openAdmin(Player&, std::string const&) override {}
    };
    SelectionProbe() {
        auto probe = std::make_unique<Probe>();
        mProbe     = probe.get();
        mSaved     = std::move(service::detail::slot<service::ISelection>());
        service::detail::slot<service::ISelection>() = std::move(probe);
    }
    ~SelectionProbe() { service::detail::slot<service::ISelection>() = std::move(mSaved); }
    Probe* operator->() const { return mProbe; }

private:
    Probe*                               mProbe = nullptr;
    std::unique_ptr<service::ISelection> mSaved;
};

/// Restores the `items` config after a test changed it.
struct ConfigGuard {
    ItemsConfig saved = config::get<ItemsConfig>();
    ~ConfigGuard() { config::mut<ItemsConfig>() = saved; }
};

} // namespace

BS_REGISTER_SELFTEST("scrolls.bind_blank", [](Context& ctx) {
    auto& p   = survivalPlayer(ctx);
    auto& rec = ctx.createWaystone(record(ctx, 400, "Bind Target"), true);
    ctx.check(recordAtBlock(p, rec.pos) == &rec, "the record block is found");
    ctx.check(recordAtBlock(p, ctx.testPos(402)) == nullptr, "a plain position has no record");
    giveHand(ctx, p, item(ids::kBlankScroll, 2));
    ctx.require(bindBlankScroll(p, rec.uid), "binding succeeds");
    ctx.check(game::idOf(hand(p)) == ids::id(ids::kBlankScroll) && hand(p).mCount == 1, "one blank scroll consumed");
    auto loc = game::findItem(p, [](ItemStack const& s) { return game::idOf(s) == ids::id(ids::kBoundScroll); });
    ctx.require(loc.has_value(), "a bound scroll was given");
    auto const& bound = game::itemAt(p, *loc);
    ctx.check(game::userString(bound, ids::kNbtBoundUid) == rec.uid, "bound uid");
    ctx.check(game::userString(bound, ids::kNbtBoundName) == "Bind Target", "bound name");
    auto lore = bound.getCustomLore();
    ctx.check(!lore.empty() && lore.front().find("Bind Target") != std::string::npos, "lore shows the bound name");
    ctx.check(!bindBlankScroll(p, "no-such-uid"), "unknown records do not bind");
});

BS_REGISTER_SELFTEST("scrolls.bound_warp", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = survivalPlayer(ctx);
    auto& rec = ctx.createWaystone(record(ctx, 410, "Bound Target"), true);
    auto  s   = item(ids::kBoundScroll, 2);
    game::setUserString(s, ids::kNbtBoundUid, rec.uid);
    giveHand(ctx, p, s);
    completeCharge(p, ScrollItem::BoundScroll);
    co_await ctx.ticks(2);
    ctx.check(near(p, rec), "the player was warped to the bound waystone");
    ctx.check(countOf(p, ids::kBoundScroll) == 1, "the bound scroll was consumed");
    // An unbound scroll does nothing.
    auto unbound = item(ids::kBoundScroll);
    giveHand(ctx, p, unbound);
    completeCharge(p, ScrollItem::BoundScroll);
    ctx.check(countOf(p, ids::kBoundScroll) == 1, "an unbound scroll is kept");
});

BS_REGISTER_SELFTEST("scrolls.return_scroll", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p    = survivalPlayer(ctx);
    auto& ws   = game::Waystones::get();
    auto& a    = ctx.createWaystone(record(ctx, 420, "Near"), true);
    auto& b    = ctx.createWaystone(record(ctx, 440, "Far"), true);
    auto  aUid = a.uid, bUid = b.uid;
    ws.activate(p, bUid);
    ws.activate(p, aUid);
    auto& data = ws.playerData(p);
    data.extra.erase("returnPortal");
    ctx.check(returnTargetFor(p) == aUid, "nearest activated waystone");
    auto& portal = ctx.createWaystone(record(ctx, 460, "Portal", std::string(core::types::kWarpPortal)));
    data.extra["returnPortal"] = portal.uid;
    ctx.check(returnTargetFor(p) == portal.uid, "the return portal wins");
    data.extra["returnPortal"] = bUid; // not a warp portal record
    ctx.check(returnTargetFor(p) == aUid, "a non-portal return uid falls back to the nearest waystone");
    data.extra.erase("returnPortal");
    giveHand(ctx, p, item(ids::kReturnScroll, 2));
    completeCharge(p, ScrollItem::ReturnScroll);
    co_await ctx.ticks(2);
    ctx.check(near(p, a), "warped to the nearest waystone");
    ctx.check(countOf(p, ids::kReturnScroll) == 1, "the return scroll was consumed");
    ws.forget(p, aUid);
    ws.forget(p, bUid);
});

BS_REGISTER_SELFTEST("scrolls.warp_stone_durability", [](Context& ctx) {
    ConfigGuard guard;
    auto&       p   = survivalPlayer(ctx);
    auto&       rec = ctx.createWaystone(record(ctx, 470, "Stone Target"), true);
    giveHand(ctx, p, item(ids::kWarpStone));
    auto& tp = service::get<service::ITeleport>();
    config::mut<ItemsConfig>().enableDurability = true;
    ctx.require(tp.warp(p, itemWarp(core::WarpSourceKind::WarpStone, ids::kWarpStone, rec.uid)).ok, "warp ok");
    ctx.check(game::idOf(hand(p)) == ids::id(ids::kWarpStone) && hand(p).getDamageValue() == 1, "warp stone lost 1 durability");
    config::mut<ItemsConfig>().enableDurability = false;
    ctx.require(tp.warp(p, itemWarp(core::WarpSourceKind::WarpStone, ids::kWarpStone, rec.uid)).ok, "warp ok");
    ctx.check(hand(p).getDamageValue() == 1, "no durability loss when enableDurability is off");
    // Scrolls are consumed by the listener too, from the hand that started the warp.
    giveHand(ctx, p, item(ids::kWarpScroll, 3));
    ctx.require(tp.warp(p, itemWarp(core::WarpSourceKind::WarpScroll, ids::kWarpScroll, rec.uid)).ok, "warp ok");
    ctx.check(hand(p).mCount == 2, "warp scroll consumed after the warp");
});

BS_REGISTER_SELFTEST("scrolls.creative_keeps", [](Context& ctx) {
    ConfigGuard guard;
    config::mut<ItemsConfig>().enableDurability = true;
    auto& p = ctx.player();
    p.setPlayerGameType(GameType::Creative);
    auto& rec = ctx.createWaystone(record(ctx, 480, "Creative Target"), true);
    auto& tp  = service::get<service::ITeleport>();
    giveHand(ctx, p, item(ids::kWarpScroll, 3));
    ctx.require(tp.warp(p, itemWarp(core::WarpSourceKind::WarpScroll, ids::kWarpScroll, rec.uid)).ok, "warp ok");
    ctx.check(hand(p).mCount == 3, "creative keeps the scroll");
    giveHand(ctx, p, item(ids::kWarpStone));
    ctx.require(tp.warp(p, itemWarp(core::WarpSourceKind::WarpStone, ids::kWarpStone, rec.uid)).ok, "warp ok");
    ctx.check(hand(p).getDamageValue() == 0, "creative keeps the warp stone's durability");
    p.setPlayerGameType(GameType::Survival);
});

BS_REGISTER_SELFTEST("scrolls.warp_scroll_selection", [](Context& ctx) {
    auto&          p = survivalPlayer(ctx);
    SelectionProbe probe;
    giveHand(ctx, p, item(ids::kWarpScroll, 2));
    completeCharge(p, ScrollItem::WarpScroll);
    ctx.check(probe->opened == 1, "the warp scroll opens the selection");
    ctx.check(probe->last.kind == core::WarpSourceKind::WarpScroll && probe->last.itemId == ids::id(ids::kWarpScroll), "source = warp scroll");
    ctx.check(countOf(p, ids::kWarpScroll) == 2, "opening the selection consumes nothing");
    giveHand(ctx, p, item(ids::kWarpStone));
    completeCharge(p, ScrollItem::WarpStone);
    ctx.check(probe->opened == 2 && probe->last.kind == core::WarpSourceKind::WarpStone, "the warp stone opens the selection");
});

BS_REGISTER_SELFTEST("scrolls.charge", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard guard;
    config::mut<ItemsConfig>().scrollUseTime = 6;
    auto&          p = survivalPlayer(ctx);
    SelectionProbe probe;
    giveHand(ctx, p, item(ids::kWarpScroll, 2));
    ctx.require(startCharge(p), "the charge starts");
    ctx.check(!startCharge(p), "no second charge while charging");
    co_await ctx.ticks(3);
    ctx.check(isCharging(p), "still charging after 3 ticks");
    co_await ctx.ticks(6);
    ctx.check(!isCharging(p), "the charge completed");
    ctx.check(probe->opened == 1, "a completed charge opens the selection");
    // Taking damage cancels the charge.
    ctx.require(startCharge(p), "a new charge starts");
    co_await ctx.ticks(2);
    p.hurtByCause(1.f);
    ctx.check(!isCharging(p), "damage cancels the charge");
    co_await ctx.ticks(8);
    ctx.check(probe->opened == 1, "a cancelled charge opens nothing");
    // Switching away from the item cancels it too.
    ctx.require(startCharge(p), "a third charge starts");
    game::setItemAt(p, game::handSlot(p, 0), ItemStack::EMPTY_ITEM());
    co_await ctx.ticks(2);
    ctx.check(!isCharging(p), "dropping the item cancels the charge");
});

} // namespace bs::feature::scrolls
