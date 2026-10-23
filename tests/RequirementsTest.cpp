#include <gtest/gtest.h>

#include "core/requirements/Requirements.h"

using namespace bs::core;
using namespace bs::core::requirements;

namespace {

Context waystoneToWaystone(double distance) {
    Context c;
    c.sourceKind = WarpSourceKind::Waystone;
    c.sourceType = "waystone";
    c.targetType = "waystone";
    c.distance   = distance;
    return c;
}

Requirement run(std::vector<std::string> const& rules, Context const& ctx, Options opts = {}) {
    auto p = parse(rules);
    EXPECT_TRUE(p.errors.empty()) << (p.errors.empty() ? "" : p.errors.front());
    return evaluate(p, ctx, opts).toRequirement();
}

Requirement runDefault(Context const& ctx, Options opts = {}) { return run(defaultRules(), ctx, opts); }

} // namespace

// ---- names ---------------------------------------------------------------------------------------------

TEST(Requirements, NegatedNames) {
    EXPECT_EQ(negatedName("is_interdimensional"), "is_not_interdimensional");
    EXPECT_EQ(negatedName("source_is_warp_plate"), "source_is_not_warp_plate");
    EXPECT_EQ(negatedName("target_is_red_sharestone"), "target_is_not_red_sharestone");
    EXPECT_EQ(negatedName("has_cooldown"), "not_has_cooldown");
    EXPECT_EQ(negatedName("involves_dimension"), "not_involves_dimension");
    EXPECT_EQ(negatedName("source_name_equals"), "not_source_name_equals");
}

TEST(Requirements, Ids) {
    EXPECT_EQ(cooldownId("inventory_button"), "waystones:inventory_button");
    EXPECT_EQ(cooldownId("mymod:x"), "mymod:x");
    EXPECT_EQ(itemId("diamond"), "minecraft:diamond");
    EXPECT_EQ(itemId("waystones:warp_dust"), "bedstones:warp_dust");
    EXPECT_EQ(entityId("horse"), "minecraft:horse");
    EXPECT_EQ(dimensionId("minecraft:the_nether"), 1);
    EXPECT_EQ(dimensionId("the_end"), 2);
    EXPECT_EQ(dimensionId("overworld"), 0);
    EXPECT_EQ(dimensionId("7"), 7);
    EXPECT_FALSE(dimensionId("mars").has_value());
}

TEST(Requirements, EveryConditionHasANegatedForm) {
    for (auto const& name : conditionNames()) {
        // Conditions with arguments fail the argument check, but never as an unknown name.
        std::string err;
        (void)parseRule("[" + negatedName(name) + "] dismount()", &err);
        EXPECT_EQ(err.find("unknown condition"), std::string::npos) << name;
    }
    EXPECT_EQ(conditionNames().size(), 38u + 30u); // 38 plain + 15 colors x source/target
}

// ---- parser -----------------------------------------------------------------------------------------------

TEST(Requirements, ParsesDefaultList) {
    auto p = parse(defaultRules());
    EXPECT_TRUE(p.errors.empty());
    ASSERT_EQ(p.rules.size(), 9u);
    EXPECT_EQ(p.rules[0].conditions.size(), 1u);
    EXPECT_EQ(p.rules[0].conditions[0].name, "is_not_interdimensional");
    EXPECT_EQ(p.rules[0].function.name, "scaled_add_xp_cost");
    EXPECT_EQ(p.rules[0].function.args, (std::vector<std::string>{"distance", "0.01"}));
    EXPECT_TRUE(p.rules[6].conditions.empty());
    EXPECT_EQ(p.rules[8].function.args, (std::vector<std::string>{"inventory_button", "300"}));
}

TEST(Requirements, ParserSyntax) {
    std::string err;
    auto        r = parseRule("  [ has_item( diamond , 2 ),is_on_vehicle(#horse)]  waystones:add_item_cost(diamond, 2) ", &err);
    ASSERT_TRUE(r) << err;
    EXPECT_EQ(r->conditions.size(), 2u);
    EXPECT_EQ(r->conditions[0].args, (std::vector<std::string>{"diamond", "2"}));
    EXPECT_EQ(r->conditions[1].args, (std::vector<std::string>{"#horse"}));
    EXPECT_EQ(r->function.name, "add_item_cost");

    auto q = parseRule(R"([target_name_contains("a, b")] refuse("$chat.waystones.cannot_dimension_warp"))", &err);
    ASSERT_TRUE(q) << err;
    EXPECT_EQ(q->conditions[0].args[0], "a, b");
    EXPECT_EQ(q->function.args[0], "$chat.waystones.cannot_dimension_warp");

    EXPECT_TRUE(parseRule("dismount"));
    EXPECT_TRUE(parseRule("[] dismount()"));
}

TEST(Requirements, ParserErrors) {
    std::string err;
    EXPECT_FALSE(parseRule("", &err));
    EXPECT_FALSE(parseRule("[is_interdimensional add_xp_cost(1)", &err));
    EXPECT_NE(err.find("']'"), std::string::npos);
    EXPECT_FALSE(parseRule("[is_bogus] add_xp_cost(1)", &err));
    EXPECT_NE(err.find("unknown condition"), std::string::npos);
    EXPECT_FALSE(parseRule("frobnicate(1)", &err));
    EXPECT_NE(err.find("unknown function"), std::string::npos);
    EXPECT_FALSE(parseRule("add_xp_cost()", &err));
    EXPECT_FALSE(parseRule("add_xp_cost(1.5)", &err)); // integer argument
    EXPECT_FALSE(parseRule("multiply_xp_cost(abc)", &err));
    EXPECT_FALSE(parseRule("scaled_add_xp_cost(speed, 1)", &err)); // unknown variable
    EXPECT_FALSE(parseRule("[source_is_dimension(mars)] dismount()", &err));
    EXPECT_FALSE(parseRule("add_xp_cost(1) trailing", &err));
    EXPECT_FALSE(parseRule("add_xp_cost(1", &err));
    EXPECT_FALSE(parseRule("add_xp_cost(1,)", &err));
    EXPECT_FALSE(parseRule("add_item_cost(#planks, 1)", &err)); // no item tags
    EXPECT_FALSE(parseRule("[has_item(#planks, 1)] dismount()", &err));
    EXPECT_FALSE(parseRule("[is_on_vehicle(#)] dismount()", &err)) << "a bare # is no family";
    auto p = parse({"", "add_xp_cost(1)", "bad line(", "max_xp_cost(5)"});
    EXPECT_EQ(p.rules.size(), 2u);
    EXPECT_EQ(p.errors.size(), 1u);
}

// ---- default list -----------------------------------------------------------------------------------------

TEST(Requirements, DefaultSameDimensionScalesWithDistance) {
    EXPECT_EQ(runDefault(waystoneToWaystone(0)).xpPoints, 0);
    EXPECT_EQ(runDefault(waystoneToWaystone(99)).xpPoints, 0); // 0.99 truncated
    EXPECT_EQ(runDefault(waystoneToWaystone(150)).xpPoints, 1);
    EXPECT_EQ(runDefault(waystoneToWaystone(1000)).xpPoints, 10);
    EXPECT_EQ(runDefault(waystoneToWaystone(2500)).xpPoints, 25);
    EXPECT_EQ(runDefault(waystoneToWaystone(100000)).xpPoints, 27); // capped
    auto r = runDefault(waystoneToWaystone(1000));
    EXPECT_EQ(r.levels, 0);
    EXPECT_TRUE(r.cooldowns.empty());
    EXPECT_TRUE(r.items.empty());
}

TEST(Requirements, DefaultInterdimensional) {
    auto c      = waystoneToWaystone(5);
    c.targetDim = 1;
    EXPECT_EQ(runDefault(c).xpPoints, 27);
}

TEST(Requirements, DefaultFreeSources) {
    for (auto kind : {WarpSourceKind::WarpScroll, WarpSourceKind::BoundScroll, WarpSourceKind::ReturnScroll, WarpSourceKind::PortalScroll}) {
        auto c       = waystoneToWaystone(2000);
        c.sourceKind = kind;
        c.sourceType = "";
        EXPECT_EQ(runDefault(c).xpPoints, 0) << toString(kind);
    }
    auto plate       = waystoneToWaystone(2000);
    plate.sourceKind = WarpSourceKind::WarpPlate;
    plate.sourceType = "warp_plate";
    EXPECT_EQ(runDefault(plate).xpPoints, 0);
    auto global         = waystoneToWaystone(2000);
    global.targetGlobal = true;
    EXPECT_EQ(runDefault(global).xpPoints, 0);
    auto memorial       = waystoneToWaystone(2000);
    memorial.targetType = "fleeting_memorial";
    EXPECT_EQ(runDefault(memorial).xpPoints, 0);
    // Not free: warp stone, sharestone, inventory button.
    auto stone       = waystoneToWaystone(2000);
    stone.sourceKind = WarpSourceKind::WarpStone;
    stone.sourceType = "";
    EXPECT_EQ(runDefault(stone).xpPoints, 20);
}

TEST(Requirements, DefaultInventoryButtonCooldown) {
    auto c       = waystoneToWaystone(1000);
    c.sourceKind = WarpSourceKind::InventoryButton;
    c.sourceType = "";
    auto r       = runDefault(c);
    EXPECT_EQ(r.xpPoints, 10);
    ASSERT_EQ(r.cooldowns.size(), 1u);
    EXPECT_EQ(r.cooldowns[0].id, "waystones:inventory_button");
    EXPECT_DOUBLE_EQ(r.cooldowns[0].seconds, 300);
    EXPECT_FALSE(r.cooldowns[0].soft);
}

TEST(Requirements, KillSwitches) {
    auto c       = waystoneToWaystone(1000);
    c.sourceKind = WarpSourceKind::InventoryButton;
    auto noCosts = runDefault(c, {false, true});
    EXPECT_EQ(noCosts.xpPoints, 0);
    EXPECT_EQ(noCosts.cooldowns.size(), 1u);
    auto noCd = runDefault(c, {true, false});
    EXPECT_EQ(noCd.xpPoints, 10);
    EXPECT_TRUE(noCd.cooldowns.empty());
    // refuse and dismount are neither costs nor cooldowns.
    auto r = run({"refuse(no)", "dismount()"}, c, {false, false});
    EXPECT_EQ(r.refuse, "no");
    EXPECT_TRUE(r.dismount);
}

// ---- functions ------------------------------------------------------------------------------------------

TEST(Requirements, LevelFunctions) {
    auto c = waystoneToWaystone(250);
    EXPECT_EQ(run({"add_level_cost(2.7)"}, c).levels, 2);
    EXPECT_EQ(run({"add_level_cost(3)", "multiply_level_cost(1.5)"}, c).levels, 4);
    EXPECT_EQ(run({"scaled_add_level_cost(distance, 0.02)"}, c).levels, 5);
    EXPECT_EQ(run({"add_level_cost(2)", "scaled_multiply_level_cost(distance, 0.01)"}, c).levels, 5);
    EXPECT_EQ(run({"min_level_cost(3)"}, c).levels, 3);
    EXPECT_EQ(run({"add_level_cost(10)", "max_level_cost(4)"}, c).levels, 4);
    EXPECT_EQ(run({"add_level_cost(-5)"}, c).levels, 0); // clamped
}

TEST(Requirements, XpFunctionsApplyInOrder) {
    auto c = waystoneToWaystone(1000);
    EXPECT_EQ(run({"add_xp_cost(5)", "multiply_xp_cost(2)"}, c).xpPoints, 10);
    EXPECT_EQ(run({"multiply_xp_cost(2)", "add_xp_cost(5)"}, c).xpPoints, 5);
    EXPECT_EQ(run({"max_xp_cost(3)", "add_xp_cost(5)"}, c).xpPoints, 5); // max only clamps what came before
    EXPECT_EQ(run({"add_xp_cost(5)", "max_xp_cost(3)"}, c).xpPoints, 3);
    EXPECT_EQ(run({"add_xp_cost(-5)", "min_xp_cost(0)"}, c).xpPoints, 0);
    EXPECT_EQ(run({"min_xp_cost(7)"}, c).xpPoints, 7);
}

TEST(Requirements, CooldownFunctions) {
    auto c   = waystoneToWaystone(100);
    c.pets   = 2;
    auto r   = run({"add_cooldown(a, 10)", "multiply_cooldown(a, 3)", "scaled_add_cooldown(pets, a, 5)", "max_cooldown(a, 35)",
                    "min_cooldown(b, 4)", "add_soft_cooldown(a, 7)", "scaled_multiply_soft_cooldown(pets, a, 2)"},
                   c);
    ASSERT_EQ(r.cooldowns.size(), 3u);
    EXPECT_EQ(r.cooldowns[0].id, "waystones:a");
    EXPECT_DOUBLE_EQ(r.cooldowns[0].seconds, 35);
    EXPECT_EQ(r.cooldowns[1].id, "waystones:b");
    EXPECT_DOUBLE_EQ(r.cooldowns[1].seconds, 4);
    EXPECT_TRUE(r.cooldowns[2].soft);
    EXPECT_DOUBLE_EQ(r.cooldowns[2].seconds, 28);
}

TEST(Requirements, ItemFunctions) {
    auto c    = waystoneToWaystone(100);
    c.leashed = 3;
    auto r    = run({"add_item_cost(diamond, 1)", "scaled_add_item_cost(leashed, diamond, 2)", "add_item_cost(waystones:warp_dust, 4)",
                     "multiply_item_cost(waystones:warp_dust, 0.5)", "add_item_cost(stick, 5)", "max_item_cost(stick, 0)"},
                    c);
    ASSERT_EQ(r.items.size(), 2u); // stick capped to 0 and dropped
    EXPECT_EQ(r.items[0].id, "minecraft:diamond");
    EXPECT_EQ(r.items[0].count, 7);
    EXPECT_EQ(r.items[1].id, "bedstones:warp_dust");
    EXPECT_EQ(r.items[1].count, 2);
}

TEST(Requirements, RefuseAndDismount) {
    auto c      = waystoneToWaystone(10);
    c.targetDim = 2;
    auto r      = run({"[is_interdimensional] refuse($chat.waystones.cannot_dimension_warp)", "[is_on_any_vehicle] dismount()"}, c);
    EXPECT_EQ(r.refuse, "$chat.waystones.cannot_dimension_warp");
    EXPECT_FALSE(r.dismount);
    EXPECT_FALSE(r.free());
    c.onVehicle = true;
    EXPECT_TRUE(run({"[is_on_any_vehicle] dismount()"}, c).dismount);
}

// ---- conditions -----------------------------------------------------------------------------------------

namespace {
bool cond(std::string const& text, Context const& c) {
    std::string err;
    auto        r = parseRule("[" + text + "] dismount()", &err);
    EXPECT_TRUE(r) << text << ": " << err;
    return r && testCondition(r->conditions[0], c);
}
} // namespace

TEST(Requirements, SourceConditions) {
    Context c;
    c.sourceKind = WarpSourceKind::Sharestone;
    c.sourceType = "red_sharestone";
    c.sourceName = "Market Hall";
    c.sourceDim  = 1;
    EXPECT_TRUE(cond("source_is_sharestone", c));
    EXPECT_TRUE(cond("source_is_red_sharestone", c));
    EXPECT_FALSE(cond("source_is_blue_sharestone", c));
    EXPECT_TRUE(cond("source_is_not_blue_sharestone", c));
    EXPECT_FALSE(cond("source_is_waystone", c));
    EXPECT_TRUE(cond("source_name_equals(Market Hall)", c));
    EXPECT_TRUE(cond("source_name_contains(Hall)", c));
    EXPECT_TRUE(cond("not_source_name_contains(Tower)", c));
    EXPECT_TRUE(cond("source_is_dimension(the_nether)", c));
    EXPECT_FALSE(cond("source_is_dimension(minecraft:overworld)", c));
    c = {};
    c.sourceKind = WarpSourceKind::ReturnScroll;
    EXPECT_TRUE(cond("source_is_scroll", c));
    EXPECT_TRUE(cond("source_is_return_scroll", c));
    EXPECT_FALSE(cond("source_is_bound_scroll", c));
    c.sourceKind = WarpSourceKind::Portstone;
    EXPECT_TRUE(cond("source_is_portstone", c));
    c.sourceKind = WarpSourceKind::WarpStone;
    EXPECT_TRUE(cond("source_is_warp_stone", c));
    EXPECT_FALSE(cond("source_is_scroll", c));
    c.sourceKind = WarpSourceKind::InventoryButton;
    EXPECT_TRUE(cond("source_is_inventory_button", c));
    c.sourceKind = WarpSourceKind::WarpPlate;
    EXPECT_TRUE(cond("source_is_warp_plate", c));
}

TEST(Requirements, TargetConditions) {
    Context c;
    c.targetType = "warp_portal";
    c.targetName = "Spawn";
    c.targetDim  = 2;
    EXPECT_TRUE(cond("target_is_warp_portal", c));
    EXPECT_FALSE(cond("target_is_waystone", c));
    EXPECT_TRUE(cond("target_name_equals(Spawn)", c));
    EXPECT_TRUE(cond("target_is_dimension(the_end)", c));
    EXPECT_TRUE(cond("target_is_not_global", c));
    c.targetType = "player";
    EXPECT_TRUE(cond("target_is_player", c));
    c.targetType = "lime_sharestone";
    EXPECT_TRUE(cond("target_is_sharestone", c));
    EXPECT_TRUE(cond("target_is_lime_sharestone", c));
    c.targetType = "warp_plate";
    EXPECT_TRUE(cond("target_is_warp_plate", c));
}

TEST(Requirements, ContextConditions) {
    Context c;
    c.playerDim = 0;
    c.sourceDim = 0;
    c.targetDim = 1;
    c.distance  = 50;
    EXPECT_TRUE(cond("is_interdimensional", c));
    EXPECT_TRUE(cond("involves_dimension(the_nether)", c));
    EXPECT_FALSE(cond("involves_dimension(the_end)", c));
    EXPECT_FALSE(cond("is_within_distance(100)", c)); // other dimension
    c.targetDim = 0;
    EXPECT_TRUE(cond("is_not_interdimensional", c));
    EXPECT_TRUE(cond("is_within_distance(50)", c));
    EXPECT_FALSE(cond("is_within_distance(49.5)", c));
}

TEST(Requirements, EntityConditions) {
    Context c;
    EXPECT_FALSE(cond("is_on_any_vehicle", c));
    EXPECT_FALSE(cond("is_on_vehicle(horse)", c));
    c.onVehicle       = true;
    c.vehicleType     = "minecraft:horse";
    c.vehicleFamilies = {"horse", "mob"};
    EXPECT_TRUE(cond("is_on_any_vehicle", c));
    EXPECT_TRUE(cond("is_on_vehicle(horse)", c));
    EXPECT_TRUE(cond("is_on_vehicle(minecraft:horse)", c));
    EXPECT_FALSE(cond("is_on_vehicle(boat)", c));
    EXPECT_TRUE(cond("is_on_vehicle(#mob)", c));
    EXPECT_TRUE(cond("is_not_on_vehicle(#minecraft:boat)", c));
    EXPECT_FALSE(cond("is_with_pets", c));
    c.pets       = 1;
    c.passengers = 2;
    c.leashed    = 1;
    EXPECT_TRUE(cond("is_with_pets", c));
    EXPECT_TRUE(cond("is_with_passengers", c));
    EXPECT_TRUE(cond("is_with_leashed", c));
    EXPECT_FALSE(cond("is_not_with_leashed", c));
}

TEST(Requirements, PlayerStateConditions) {
    Context c;
    c.cooldownRemainingMs["waystones:inventory_button"] = 12'000;
    c.itemCounts["minecraft:diamond"]                   = 3;
    c.emptyInventory                                     = false;
    c.wearingArmor                                       = true;
    c.tags                                               = {"vip"};
    EXPECT_TRUE(cond("has_cooldown(inventory_button)", c));
    EXPECT_TRUE(cond("has_cooldown_above(inventory_button, 10)", c));
    EXPECT_FALSE(cond("has_cooldown_above(inventory_button, 12)", c));
    EXPECT_TRUE(cond("not_has_cooldown(other)", c));
    EXPECT_TRUE(cond("has_item(diamond, 3)", c));
    EXPECT_FALSE(cond("has_item(diamond, 4)", c));
    EXPECT_FALSE(cond("has_empty_inventory", c));
    EXPECT_TRUE(cond("not_has_empty_inventory", c));
    EXPECT_TRUE(cond("is_wearing_any_armor", c));
    EXPECT_TRUE(cond("has_tag(vip)", c));
    EXPECT_FALSE(cond("has_tag(admin)", c));
}

TEST(Requirements, MultipleConditionsAreAnded) {
    auto c   = waystoneToWaystone(10);
    c.pets   = 1;
    auto yes = run({"[is_with_pets, is_not_interdimensional] add_xp_cost(4)"}, c);
    EXPECT_EQ(yes.xpPoints, 4);
    auto no = run({"[is_with_pets, is_interdimensional] add_xp_cost(4)"}, c);
    EXPECT_EQ(no.xpPoints, 0);
}

TEST(Requirements, ActiveCooldown) {
    Requirement r;
    r.cooldowns.push_back({"waystones:inventory_button", 300, false});
    r.cooldowns.push_back({"waystones:soft", 300, true});
    std::map<std::string, std::int64_t> active{{"waystones:soft", 99'000}};
    EXPECT_DOUBLE_EQ(activeCooldownSeconds(r, active), 0); // soft cooldowns never block
    active["waystones:inventory_button"] = 4'500;
    EXPECT_DOUBLE_EQ(activeCooldownSeconds(r, active), 4.5);
    active["waystones:inventory_button"] = -1;
    EXPECT_DOUBLE_EQ(activeCooldownSeconds(r, active), 0);
}
