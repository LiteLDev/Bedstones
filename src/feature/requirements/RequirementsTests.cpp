// U8 selftests (`/waystones selftest "requirements.*"`). They resolve requirements against plain
// list entries (no blocks), so they need no test area; consume / XP tests use the simulated player.

#include <cmath>

#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/GameType.h"

#include "feature/requirements/Requirements.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/Config.h"
#include "net/Companion.h"
#include "net/Messages.h"
#include "selftest/SelfTest.h"
#include "service/Services.h"

namespace bs::feature::requirements {

namespace {

using selftest::Context;

service::IRequirements& seam() { return service::get<service::IRequirements>(); }

/// A list entry `dx` blocks east of the player (same y), in dimension `dim`.
core::WarpTarget targetAt(Player& p, int dx, int dim = -1) {
    auto const&      at = p.getPosition();
    core::WarpTarget t;
    t.id   = "requirements-test";
    t.kind = "waystone";
    t.pos  = {dim < 0 ? static_cast<int>(p.getDimensionId()) : dim, static_cast<int>(std::floor(at.x)) + dx,
              static_cast<int>(std::floor(at.y)), static_cast<int>(std::floor(at.z))};
    return t;
}

core::WarpSource sourceOf(core::WarpSourceKind kind) {
    core::WarpSource s;
    s.kind = kind;
    return s;
}

/// Restores the config section and the player's game mode / cooldowns after a test.
struct Restore {
    Player&            player;
    RequirementsConfig config   = config::get<RequirementsConfig>();
    GameType           gameType = player.getPlayerGameType();
    explicit Restore(Player& p) : player(p) {}
    ~Restore() {
        config::mut<RequirementsConfig>() = config;
        player.setPlayerGameType(gameType);
        resetCooldowns(player);
    }
};

void defaultsOnly() {
    auto& c           = config::mut<RequirementsConfig>();
    c                 = RequirementsConfig{};
}

void survival(Player& p) { p.setPlayerGameType(GameType::Survival); }

} // namespace

BS_REGISTER_SELFTEST("requirements.default_costs", [](Context& ctx) {
    auto&   p = ctx.player();
    Restore restore(p);
    defaultsOnly();
    auto src  = sourceOf(core::WarpSourceKind::Waystone);
    auto cost = [&](int dx) { return seam().resolve(p, src, targetAt(p, dx)).xpPoints; };
    ctx.check(cost(0) == 0, "0 blocks is free");
    ctx.check(cost(50) == 0, "50 blocks cost 0 points (0.5 truncated)");
    ctx.check(cost(150) == 1, "150 blocks cost 1 point, got " + std::to_string(cost(150)));
    ctx.check(cost(1050) == 10, "1050 blocks cost 10 points, got " + std::to_string(cost(1050)));
    ctx.check(cost(2550) == 25, "2550 blocks cost 25 points, got " + std::to_string(cost(2550)));
    ctx.check(cost(10000) == 27, "far warps are capped at 27 points, got " + std::to_string(cost(10000)));
    auto r = seam().resolve(p, src, targetAt(p, 1050));
    ctx.check(r.levels == 0 && r.items.empty() && r.cooldowns.empty() && r.refuse.empty(), "only an XP cost");
});

BS_REGISTER_SELFTEST("requirements.cross_dimension", [](Context& ctx) {
    auto&   p = ctx.player();
    Restore restore(p);
    defaultsOnly();
    int  other = static_cast<int>(p.getDimensionId()) == 1 ? 0 : 1;
    auto r     = seam().resolve(p, sourceOf(core::WarpSourceKind::Waystone), targetAt(p, 5, other));
    ctx.check(r.xpPoints == 27, "changing dimension costs 27 points, got " + std::to_string(r.xpPoints));
    auto global   = targetAt(p, 5, other);
    global.global = true;
    ctx.check(seam().resolve(p, sourceOf(core::WarpSourceKind::Waystone), global).xpPoints == 0, "global targets are free");
});

BS_REGISTER_SELFTEST("requirements.free_sources", [](Context& ctx) {
    auto&   p = ctx.player();
    Restore restore(p);
    defaultsOnly();
    auto t = targetAt(p, 2050);
    for (auto k : {core::WarpSourceKind::WarpScroll, core::WarpSourceKind::BoundScroll, core::WarpSourceKind::ReturnScroll,
                   core::WarpSourceKind::PortalScroll, core::WarpSourceKind::WarpPlate}) {
        ctx.check(seam().resolve(p, sourceOf(k), t).xpPoints == 0, std::string(core::toString(k)) + " is free");
    }
    auto memorial = t;
    memorial.kind = "fleeting_memorial";
    memorial.id.clear();
    ctx.check(seam().resolve(p, sourceOf(core::WarpSourceKind::Waystone), memorial).xpPoints == 0, "memorial targets are free");
    ctx.check(seam().resolve(p, sourceOf(core::WarpSourceKind::WarpStone), t).xpPoints == 20, "the warp stone pays 20 points");
    // A source waystone record: its record type drives source conditions.
    core::WaystoneRecord plate;
    plate.type = std::string(core::types::kWarpPlate);
    plate.name = "Plate";
    plate.pos  = ctx.testPos(800);
    auto& rec  = ctx.createWaystone(plate);
    core::WarpSource fromPlate;
    fromPlate.kind    = core::WarpSourceKind::WarpPlate;
    fromPlate.fromUid = rec.uid;
    ctx.check(seam().resolve(p, fromPlate, t).xpPoints == 0, "warp plates are free");
});

BS_REGISTER_SELFTEST("requirements.cooldown", [](Context& ctx) {
    auto&   p = ctx.player();
    Restore restore(p);
    defaultsOnly();
    survival(p);
    resetCooldowns(p);
    int synced = 0;
    net::setTestSink([&synced](Player&, std::string_view, nlohmann::json const& body) {
        if (body.value("channel", std::string{}) == kCooldownChannel) ++synced;
    });
    net::setCompanion(p, net::CompanionInfo{"test", {}});
    auto src = sourceOf(core::WarpSourceKind::InventoryButton);
    auto r   = seam().resolve(p, src, targetAt(p, 0));
    ctx.check(r.cooldowns.size() == 1 && r.cooldowns[0].id == kInventoryButtonCooldown && r.cooldowns[0].seconds == 300,
              "inventory button warps have a 300 s cooldown");
    ctx.check(r.activeCooldown == 0 && seam().canAfford(p, r), "affordable before the first warp");
    seam().consume(p, r);
    auto const& data = game::Waystones::get().playerData(p);
    auto        it   = data.cooldowns.find(std::string(kInventoryButtonCooldown));
    ctx.require(it != data.cooldowns.end(), "cooldown stored in the player data");
    auto left = it->second - nowMs();
    ctx.check(left > 295'000 && left <= 300'000, "expiry is 300 s from now");
    ctx.check(synced >= 1, "cooldowns synced to the client mod");
    auto again = seam().resolve(p, src, targetAt(p, 0));
    ctx.check(again.activeCooldown > 295 && again.activeCooldown <= 300, "active cooldown reported");
    ctx.check(!seam().canAfford(p, again), "blocked while the cooldown runs");
    auto lines = tooltipLines(p, again);
    ctx.check(!lines.empty() && lines.back() == "§6Cooldown: 300 seconds", "tooltip shows the time left");
    ctx.check(!seam().canAfford(p, r), "a stale requirement is re-checked too");
    auto other = seam().resolve(p, sourceOf(core::WarpSourceKind::Waystone), targetAt(p, 0));
    ctx.check(other.activeCooldown == 0 && seam().canAfford(p, other), "waystone warps are not blocked by the button cooldown");
    int before = synced;
    net::simulateClientMessage(p, kCooldownRequestChannel, nlohmann::json::object());
    ctx.check(synced == before + 1, "cooldowns sent on request");
    // The kill switch drops the cooldown function.
    config::mut<RequirementsConfig>().enableCooldowns = false;
    ctx.check(seam().resolve(p, src, targetAt(p, 0)).cooldowns.empty(), "enableCooldowns=false disables cooldowns");
    net::setCompanion(p, std::nullopt);
    net::setTestSink(nullptr);
});

BS_REGISTER_SELFTEST("requirements.creative", [](Context& ctx) {
    auto&   p = ctx.player();
    Restore restore(p);
    defaultsOnly();
    survival(p);
    setTotalXp(p, 0);
    auto r = seam().resolve(p, sourceOf(core::WarpSourceKind::InventoryButton), targetAt(p, 5000));
    ctx.check(r.xpPoints == 27, "costs 27 points");
    ctx.check(!seam().canAfford(p, r), "survival without XP can't afford it");
    setCooldown(p, std::string(kInventoryButtonCooldown), 100);
    p.setPlayerGameType(GameType::Creative);
    auto blocked = seam().resolve(p, sourceOf(core::WarpSourceKind::InventoryButton), targetAt(p, 5000));
    ctx.check(blocked.activeCooldown > 0, "cooldown active");
    ctx.check(seam().canAfford(p, blocked), "creative players can always afford");
    seam().consume(p, blocked);
    ctx.check(totalXp(p) == 0, "creative players pay nothing");
    ctx.check(activeCooldowns(p).empty(), "the inventory button resets cooldowns in creative");
});

BS_REGISTER_SELFTEST("requirements.custom_rules", [](Context& ctx) {
    auto&   p = ctx.player();
    Restore restore(p);
    survival(p);
    auto& cfg            = config::mut<RequirementsConfig>();
    cfg.warpRequirements = {
        "add_level_cost(3)",
        "[is_with_pets] refuse(no pets)",
        "[source_is_warp_stone] add_item_cost(diamond, 2)",
        "add_soft_cooldown(test, 5)",
        "this is not a rule",
        "[target_name_equals(Forbidden)] refuse($chat.waystones.cannot_dimension_warp)",
    };
    ctx.check(program().rules.size() == 5 && program().errors.size() == 1, "invalid lines are skipped");
    auto src = sourceOf(core::WarpSourceKind::WarpStone);
    auto r   = seam().resolve(p, src, targetAt(p, 100));
    ctx.check(r.levels == 3 && r.xpPoints == 0, "level cost from the custom list");
    ctx.require(r.items.size() == 1 && r.items[0].id == "minecraft:diamond" && r.items[0].count == 2, "item cost");
    ctx.check(r.cooldowns.size() == 1 && r.cooldowns[0].soft, "soft cooldown");
    ctx.check(r.refuse.empty(), "no refusal");

    // Clear the inventory, then check affordability step by step.
    auto& inv = p.getInventory();
    for (int i = 0; i < inv.getContainerSize(); ++i) game::setItemAt(p, {game::ItemLocation::Kind::Inventory, i}, ItemStack::EMPTY_ITEM());
    setTotalXp(p, 0);
    ctx.check(!seam().canAfford(p, r), "no levels, no diamonds");
    p.addLevels(5);
    ctx.check(!seam().canAfford(p, r), "levels but no diamonds");
    ctx.give(p, game::makeItem("minecraft:diamond", 3));
    ctx.check(seam().canAfford(p, r), "affordable with levels and diamonds");
    seam().consume(p, r);
    ctx.check(playerLevel(p) == 2, "3 levels taken, level " + std::to_string(playerLevel(p)));
    int diamonds = 0;
    for (int i = 0; i < inv.getContainerSize(); ++i) {
        auto const& s = inv.getItem(i);
        if (!s.isNull() && game::idOf(s) == "minecraft:diamond") diamonds += s.mCount;
    }
    ctx.check(diamonds == 1, "2 diamonds taken");
    auto again = seam().resolve(p, src, targetAt(p, 100));
    ctx.check(again.activeCooldown == 0, "soft cooldowns never block");
    ctx.check(activeCooldowns(p).contains("waystones:test"), "but they start the timer");

    auto named = targetAt(p, 100);
    named.id.clear();
    named.name = "Forbidden";
    auto refused = seam().resolve(p, src, named);
    ctx.check(refused.refuse == "$chat.waystones.cannot_dimension_warp", "refuse keeps the $ translation key");
    ctx.check(!seam().canAfford(p, refused), "refused warps are unaffordable");

    cfg.enableCosts = false;
    auto noCost     = seam().resolve(p, src, targetAt(p, 100));
    ctx.check(noCost.levels == 0 && noCost.items.empty(), "enableCosts=false disables costs");
});

BS_REGISTER_SELFTEST("requirements.xp_deduction", [](Context& ctx) {
    auto&   p = ctx.player();
    Restore restore(p);
    defaultsOnly();
    survival(p);
    setTotalXp(p, 100);
    ctx.require(totalXp(p) == 100, "player has 100 points, got " + std::to_string(totalXp(p)));
    ctx.check(playerLevel(p) == 7, "100 points is level 7, got " + std::to_string(playerLevel(p)));
    auto r = seam().resolve(p, sourceOf(core::WarpSourceKind::Waystone), targetAt(p, 1550));
    ctx.require(r.xpPoints == 15, "1550 blocks cost 15 points, got " + std::to_string(r.xpPoints));
    ctx.check(seam().canAfford(p, r), "affordable");
    seam().consume(p, r);
    ctx.check(totalXp(p) == 85, "15 points deducted, now " + std::to_string(totalXp(p)));
    ctx.check(playerLevel(p) == 6, "85 points is level 6, got " + std::to_string(playerLevel(p)));
    setTotalXp(p, 10);
    ctx.check(!seam().canAfford(p, r), "10 points are not enough for 15");

    // A full warp through the teleport seam pays as well.
    core::WaystoneRecord rec;
    rec.name     = "Far";
    rec.pos      = ctx.testPos(899);
    auto& target = ctx.createWaystone(rec, true);
    setTotalXp(p, 50);
    core::WarpTarget t;
    t.id      = target.uid;
    t.kind    = "waystone";
    t.pos     = target.pos;
    auto cost = seam().resolve(p, sourceOf(core::WarpSourceKind::Waystone), t).xpPoints;
    auto res  = service::get<service::ITeleport>().warp(p, {sourceOf(core::WarpSourceKind::Waystone), target.uid});
    ctx.check(res.ok, "warp succeeded");
    ctx.check(totalXp(p) == 50 - cost, "the warp paid " + std::to_string(cost) + " points, now " + std::to_string(totalXp(p)));
});

} // namespace bs::feature::requirements
