// Selftests of the warp modifiers (`modifiers.*`): container moves through the screen actions,
// arrival effects and potency. Test area: ctx.testPos(1350..1399).

#include <cmath>

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/dimension/DimensionType.h"

#include "feature/modifiers/Modifiers.h"
#include "game/BlockContainer.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/Config.h"
#include "net/Companion.h"
#include "net/Screens.h"
#include "selftest/SelfTest.h"
#include "service/ScreenOpeners.h"
#include "service/Services.h"

namespace {

using bs::selftest::Context;
namespace screens = bs::net::screens;
namespace cm      = bs::core::modifiers;

struct CompanionScope {
    Player& p;
    explicit CompanionScope(Player& player) : p(player) {
        bs::net::setTestSink([](Player&, std::string_view, nlohmann::json const&) {});
        bs::net::setCompanion(p, bs::net::CompanionInfo{"test", {"native_screen"}});
    }
    ~CompanionScope() {
        screens::simulateAction(p, "close");
        bs::net::setCompanion(p, std::nullopt);
        bs::net::setTestSink(nullptr);
    }
};

ll::coro::CoroTask<bs::core::GlobalPos> placePlayer(Context& ctx, int dx) {
    auto& p  = ctx.player();
    auto  at = ctx.testPos(dx);
    p.teleport(Vec3{at.x + 0.5f, at.y + 1.f, at.z + 0.5f}, DimensionType(0), Vec2{0.f, 0.f});
    co_await ctx.ticks(3);
    auto feet = p.getFeetPos();
    co_return bs::core::GlobalPos{0, static_cast<int>(std::floor(feet.x)) + 1, static_cast<int>(std::floor(feet.y)), static_cast<int>(std::floor(feet.z))};
}

bs::core::WaystoneRecord record(bs::core::GlobalPos pos, std::string name) {
    bs::core::WaystoneRecord r;
    r.name   = std::move(name);
    r.pos    = pos;
    r.style  = "waystone";
    r.origin = bs::core::Origin::Player;
    return r;
}

void clearInventory(Player& p) {
    auto& inv = p.getInventory();
    for (int i = 0; i < inv.getContainerSize(); ++i) inv.setItem(i, ItemStack::EMPTY_ITEM());
}

} // namespace

BS_REGISTER_SELFTEST("modifiers.container_moves", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = ctx.player();
    auto  pos = co_await placePlayer(ctx, 1350);
    auto  uid = ctx.createWaystone(record(pos, "Box")).uid;
    clearInventory(p);
    ctx.give(p, bs::game::makeItem("minecraft:diamond", 5), 0);
    CompanionScope scope(p);
    ctx.require(bs::service::openScreen(p, "modifiers", {{"uid", uid}}), "modifiers screen opens");
    ctx.require(screens::current(p, "modifiers") != nullptr, "modifiers screen is current");

    // Shift-click: a diamond is not a shard, so it skips slot 0.
    screens::simulateAction(p, "quick_move", {{"slot", cm::kInventoryBase + 0}});
    auto c = bs::game::containerOf(*bs::game::Waystones::get().find(uid));
    ctx.check(!c[0], "slot 0 only takes shards");
    ctx.check(c[1] && c[1]->id == "minecraft:diamond" && c[1]->count == 5, "diamonds moved into slot 1");
    ctx.check(p.getInventory().getItem(0).isNull(), "inventory slot emptied");
    auto const& model = screens::current(p, "modifiers")->model;
    ctx.check(model["container"][1].is_object() && model["container"][1].value("count", 0) == 5, "model synced");

    // Click: pick up half (right), put one back (right), drop the rest into the inventory.
    screens::simulateAction(p, "click", {{"slot", 1}, {"button", 1}});
    auto cursor = screens::current(p, "modifiers")->model["cursor"];
    ctx.check(cursor.is_object() && cursor.value("count", 0) == 3, "right click picks up the larger half");
    screens::simulateAction(p, "click", {{"slot", 2}, {"button", 1}});
    c = bs::game::containerOf(*bs::game::Waystones::get().find(uid));
    ctx.check(c[2] && c[2]->count == 1, "right click places one");
    screens::simulateAction(p, "click", {{"slot", cm::kInventoryBase + 4}, {"button", 0}});
    ctx.check(p.getInventory().getItem(4).mCount == 2, "left click places the cursor stack into the inventory");
    ctx.check(screens::current(p, "modifiers")->model["cursor"].is_null(), "cursor empty");

    // Shift-click back out of the container: to the hotbar, from the end.
    screens::simulateAction(p, "quick_move", {{"slot", 1}});
    c = bs::game::containerOf(*bs::game::Waystones::get().find(uid));
    ctx.check(!c[1], "slot 1 emptied");
    ctx.check(p.getInventory().getItem(4).mCount == 4, "merged into the existing inventory stack");

    // Shard slot: one item only (needs the items unit's dormant shard).
    auto shard = bs::game::makeItem("bedstones:dormant_shard", 3);
    if (!shard.isNull()) {
        ctx.give(p, shard, 6);
        screens::simulateAction(p, "quick_move", {{"slot", cm::kInventoryBase + 6}});
        c = bs::game::containerOf(*bs::game::Waystones::get().find(uid));
        ctx.check(c[0] && c[0]->count == 1, "a single shard goes into slot 0");
    } else {
        ctx.info("bedstones:dormant_shard not registered; shard slot case skipped");
    }
    // Close returns a held item.
    screens::simulateAction(p, "click", {{"slot", 2}, {"button", 0}});
    ctx.check(screens::current(p, "modifiers")->model["cursor"].is_object(), "holding the diamond");
    screens::simulateAction(p, "close");
    co_await ctx.ticks(2); // the cursor goes back when the session is gone
    int diamonds = 0;
    auto& inv    = p.getInventory();
    for (int i = 0; i < inv.getContainerSize(); ++i) {
        if (bs::game::idOf(inv.getItem(i)) == "minecraft:diamond") diamonds += inv.getItem(i).mCount;
    }
    ctx.check(diamonds == 5, "closing returns the cursor item (" + std::to_string(diamonds) + " diamonds)");
    clearInventory(p);
});

BS_REGISTER_SELFTEST("modifiers.distance", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = ctx.player();
    auto  pos = co_await placePlayer(ctx, 1360);
    auto  far = pos;
    far.x += 10;
    auto uid = ctx.createWaystone(record(far, "Far")).uid;
    CompanionScope scope(p);
    ctx.check(!bs::feature::modifiers::openModifiers(p, uid), "more than 8 blocks away: no menu");
    far.x -= 4;
    auto uid2 = ctx.createWaystone(record(far, "Near")).uid;
    ctx.check(bs::feature::modifiers::openModifiers(p, uid2), "within 8 blocks: menu opens");
});

BS_REGISTER_SELFTEST("modifiers.arrival_effects", [](Context& ctx) {
    auto& p   = ctx.player();
    auto& rec = ctx.createWaystone(record(ctx.testPos(1370), "Hostile"));
    auto  c   = bs::game::containerOf(rec);
    c[1]      = bs::game::SlotItem{"minecraft:feather", 3, 0, {}};
    c[2]      = bs::game::SlotItem{"minecraft:poisonous_potato", 2, 0, {}};
    c[3]      = bs::game::SlotItem{"minecraft:diamond", 1, 0, {}};
    c[4]      = bs::game::SlotItem{"minecraft:ink_sac", 1, 0, {}};
    bs::game::setContainer(rec, c);
    p.removeAllEffects();
    bs::service::get<bs::service::IArrival>().onArrived(p, rec);
    auto const* slow   = p.getEffect(27u);
    auto const* poison = p.getEffect(19u);
    auto const* blind  = p.getEffect(15u);
    ctx.check(slow != nullptr, "slow falling applied");
    ctx.check(poison != nullptr, "poison applied");
    ctx.check(blind != nullptr, "blindness applied");
    if (poison) ctx.check(poison->mAmplifier == 2, "potency 2 with one diamond (got " + std::to_string(poison->mAmplifier) + ")");
    if (slow) ctx.check(slow->mDuration->mValue <= 3 * 20 && slow->mDuration->mValue > 0, "1 s per feather");

    // Cure: milk removes everything (then nothing else is added).
    c    = {};
    c[1] = bs::game::SlotItem{"minecraft:milk_bucket", 1, 0, {}};
    bs::game::setContainer(rec, c);
    bs::service::get<bs::service::IArrival>().onArrived(p, rec);
    ctx.check(p.getEffect(19u) == nullptr && p.getEffect(27u) == nullptr, "milk bucket cures effects");

    // Kill switch.
    c    = {};
    c[1] = bs::game::SlotItem{"minecraft:wither_rose", 2, 0, {}};
    bs::game::setContainer(rec, c);
    bs::config::mut<bs::feature::modifiers::ModifiersConfig>().enableModifiers = false;
    bs::service::get<bs::service::IArrival>().onArrived(p, rec);
    bs::config::mut<bs::feature::modifiers::ModifiersConfig>().enableModifiers = true;
    ctx.check(p.getEffect(20u) == nullptr, "enableModifiers=false applies nothing");
    bs::service::get<bs::service::IArrival>().onArrived(p, rec);
    ctx.check(p.getEffect(20u) != nullptr, "wither applied when enabled");
    p.removeAllEffects();
});

BS_REGISTER_SELFTEST("modifiers.potency", [](Context& ctx) {
    auto& p   = ctx.player();
    auto& rec = ctx.createWaystone(record(ctx.testPos(1380), "Potent"));
    auto  c   = bs::game::containerOf(rec);
    c[1]      = bs::game::SlotItem{"minecraft:diamond", 9, 0, {}};
    c[2]      = bs::game::SlotItem{"minecraft:magma_cream", 20, 0, {}};
    bs::game::setContainer(rec, c);
    ctx.check(cm::potency(bs::feature::modifiers::modifierItems(rec)) == 4, "potency capped at 4");
    p.removeAllEffects();
    bs::service::get<bs::service::IArrival>().onArrived(p, rec);
    auto const* res = p.getEffect(12u);
    ctx.require(res != nullptr, "fire resistance applied");
    ctx.check(res->mAmplifier == 4, "amplifier = potency");
    ctx.check(res->mDuration->mValue <= 8 * 20, "fire resistance at most 8 s");
    p.removeAllEffects();
});
