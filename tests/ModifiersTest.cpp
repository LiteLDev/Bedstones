#include <gtest/gtest.h>

#include "core/modifiers/EditRules.h"
#include "core/modifiers/Modifiers.h"

using namespace bs::core;
using namespace bs::core::modifiers;

namespace {
Stack item(std::string id, int count = 1, int maxStack = 64) {
    Stack s;
    s.id       = std::move(id);
    s.count    = count;
    s.maxStack = maxStack;
    return s;
}
} // namespace

TEST(Modifiers, PotencyFromDiamonds) {
    EXPECT_EQ(potency({}), 1);
    EXPECT_EQ(potency({item("minecraft:diamond", 1)}), 2);
    EXPECT_EQ(potency({item("minecraft:diamond", 2), item("minecraft:diamond", 1)}), 4);
    EXPECT_EQ(potency({item("minecraft:diamond", 64)}), 4);
}

TEST(Modifiers, ArrivalEffects) {
    auto e = arrivalEffects({item("minecraft:blaze_powder", 3), item("minecraft:poisonous_potato", 2), item("minecraft:diamond", 1)});
    EXPECT_FALSE(e.cure);
    EXPECT_EQ(e.fireSeconds, 3);
    ASSERT_EQ(e.effects.size(), 1u);
    EXPECT_EQ(e.effects[0], (EffectSpec{Effect::Poison, 2, 2}));

    auto capped = arrivalEffects({item("minecraft:feather", 20), item("minecraft:magma_cream", 9), item("minecraft:wither_rose", 12)});
    ASSERT_EQ(capped.effects.size(), 3u);
    EXPECT_EQ(capped.effects[0], (EffectSpec{Effect::SlowFalling, 8, 1}));
    EXPECT_EQ(capped.effects[1], (EffectSpec{Effect::FireResistance, 8, 1}));
    EXPECT_EQ(capped.effects[2], (EffectSpec{Effect::Wither, 12, 1}));

    EXPECT_TRUE(arrivalEffects({item("minecraft:milk_bucket", 1, 1)}).cure);
    EXPECT_TRUE(arrivalEffects({item("minecraft:honey_block", 1)}).cure);
    EXPECT_TRUE(arrivalEffects({item("minecraft:ink_sac", 4)}).effects[0] == (EffectSpec{Effect::Blindness, 4, 1}));
    EXPECT_TRUE(arrivalEffects({item("minecraft:stone", 4)}).empty());
}

TEST(Modifiers, ActiveCount) {
    EXPECT_EQ(activeModifierCount({}), 0);
    EXPECT_EQ(activeModifierCount({item("minecraft:amethyst_shard", 5), item("minecraft:slime_ball", 2)}), 0);
    EXPECT_EQ(activeModifierCount({item("minecraft:blaze_powder", 2), item("minecraft:diamond", 1), item("minecraft:amethyst_shard")}), 3);
}

TEST(Modifiers, Galactic) {
    EXPECT_EQ(javaHash(""), 0);
    EXPECT_EQ(javaHash("abc"), 96354);
    EXPECT_EQ(javaHash("hello world"), 1794106052);
    EXPECT_EQ(galacticIdentifier("1a2b-3c4d-ef01-abcdef"), "abcdefab");
    EXPECT_EQ(galacticIdentifier("12-34"), "");
    // |hash("abc")| % 15 = 96354 % 15 = 9 -> blue
    EXPECT_EQ(galacticColor("abc"), "blue");
    // Remapped colours never come out.
    for (auto id : {"a", "b", "c", "dd", "eeee", "fabcdefa", "abcdabcd", "ffffffff", "cafebabe"}) {
        auto c = galacticColor(id);
        EXPECT_NE(c, "gray");
        EXPECT_NE(c, "dark_gray");
        EXPECT_NE(c, "black");
    }
}

TEST(ModifierMenu, ShiftClickIntoShardSlotTakesOne) {
    Menu m;
    m.inventory[0] = item("bedstones:dormant_shard", 3, 64);
    EXPECT_TRUE(quickMove(m, kInventoryBase + 0));
    ASSERT_TRUE(m.container[0]);
    EXPECT_EQ(m.container[0]->count, 1);
    // The rest goes to the modifier slots (slot 0 is full now).
    ASSERT_TRUE(m.container[1]);
    EXPECT_EQ(m.container[1]->count, 2);
    EXPECT_FALSE(m.inventory[0]);
}

TEST(ModifierMenu, ShiftClickOtherItemsSkipShardSlot) {
    Menu m;
    m.inventory[2] = item("minecraft:diamond", 5);
    EXPECT_TRUE(quickMove(m, kInventoryBase + 2));
    EXPECT_FALSE(m.container[0]);
    ASSERT_TRUE(m.container[1]);
    EXPECT_EQ(m.container[1]->count, 5);
    // Merges into the existing stack.
    m.inventory[3] = item("minecraft:diamond", 4);
    EXPECT_TRUE(quickMove(m, kInventoryBase + 3));
    EXPECT_EQ(m.container[1]->count, 9);
    EXPECT_FALSE(m.container[2]);
}

TEST(ModifierMenu, ShiftClickBackToHotbarFromTheEnd) {
    Menu m;
    m.container[2] = item("minecraft:feather", 10);
    m.inventory[8] = item("minecraft:stone", 1);
    EXPECT_TRUE(quickMove(m, 2));
    EXPECT_FALSE(m.container[2]);
    ASSERT_TRUE(m.inventory[7]);
    EXPECT_EQ(m.inventory[7]->id, "minecraft:feather");
}

TEST(ModifierMenu, FullContainerRejects) {
    Menu m;
    for (int i = 1; i <= 4; ++i) m.container[static_cast<std::size_t>(i)] = item("minecraft:stone", 64);
    m.inventory[0] = item("minecraft:dirt", 3);
    EXPECT_FALSE(quickMove(m, kInventoryBase));
    EXPECT_EQ(m.inventory[0]->count, 3);
}

TEST(ModifierMenu, ClickPickPlaceSwap) {
    Menu m;
    m.inventory[0] = item("minecraft:diamond", 5);
    EXPECT_TRUE(click(m, kInventoryBase, 1)); // right: take the larger half
    ASSERT_TRUE(m.cursor);
    EXPECT_EQ(m.cursor->count, 3);
    EXPECT_EQ(m.inventory[0]->count, 2);
    EXPECT_FALSE(click(m, 0, 0)); // shard slot rejects diamonds
    EXPECT_TRUE(click(m, 1, 1));  // right: place one
    EXPECT_EQ(m.container[1]->count, 1);
    EXPECT_EQ(m.cursor->count, 2);
    EXPECT_TRUE(click(m, 1, 0)); // left: merge all
    EXPECT_EQ(m.container[1]->count, 3);
    EXPECT_FALSE(m.cursor);
    m.cursor = item("minecraft:feather", 2);
    EXPECT_TRUE(click(m, 1, 0)); // swap
    EXPECT_EQ(m.container[1]->id, "minecraft:feather");
    EXPECT_EQ(m.cursor->id, "minecraft:diamond");
    std::vector<Stack> dropped;
    EXPECT_TRUE(click(m, kOutside, 0, &dropped));
    EXPECT_FALSE(m.cursor);
    ASSERT_EQ(dropped.size(), 1u);
    EXPECT_EQ(dropped[0].count, 3);
}

TEST(ModifierMenu, ShardSlotHoldsOne) {
    Menu m;
    m.cursor = item("bedstones:attuned_shard", 2);
    EXPECT_TRUE(click(m, 0, 0));
    EXPECT_EQ(m.container[0]->count, 1);
    EXPECT_EQ(m.cursor->count, 1);
    EXPECT_FALSE(click(m, 0, 0)); // full
}

TEST(ModifierMenu, ReturnCursor) {
    Menu m;
    m.inventory[3] = item("minecraft:diamond", 62);
    m.cursor       = item("minecraft:diamond", 5);
    EXPECT_FALSE(returnCursor(m));
    EXPECT_FALSE(m.cursor);
    EXPECT_EQ(m.inventory[3]->count, 64);
    ASSERT_TRUE(m.inventory[0]);
    EXPECT_EQ(m.inventory[0]->count, 3);
    for (auto& s : m.inventory) s = item("minecraft:stone", 64);
    m.cursor  = item("minecraft:feather", 2);
    auto left = returnCursor(m);
    ASSERT_TRUE(left);
    EXPECT_EQ(left->count, 2);
}

TEST(EditRules, Names) {
    auto none = [](std::string const&) { return false; };
    auto all  = [](std::string const&) { return true; };
    EXPECT_EQ(edit::finalName("", "", none), "Untitled Waystone");
    EXPECT_EQ(edit::finalName("   ", "", none), "Untitled Waystone");
    EXPECT_EQ(edit::finalName("Home", "", all), "Home");
    EXPECT_EQ(edit::finalName("Home", "Home", all), "Home*");
    EXPECT_EQ(edit::finalName("Home", "Home", none), "Home");
    EXPECT_EQ(edit::finalName("NEAREST", "NEAREST", all), "NEAREST");
    EXPECT_EQ(edit::clipName(std::string(200, 'a')).size(), 128u);
    EXPECT_EQ(edit::clipName("a\nb"), "ab");
    EXPECT_EQ(edit::clipName("a\x80" "b\xE4\xBD" "c"), "abc");
    EXPECT_EQ(edit::clipName("\xE4\xBD\xA0\xE5\xA5\xBD"), "\xE4\xBD\xA0\xE5\xA5\xBD");
}

TEST(EditRules, Visibility) {
    std::vector<Visibility> opts{Visibility::Activation, Visibility::Team};
    EXPECT_EQ(edit::pickVisibility("team", opts), Visibility::Team);
    EXPECT_EQ(edit::pickVisibility("global", opts), Visibility::Activation);
    EXPECT_EQ(edit::pickVisibility("bogus", opts), Visibility::Activation);
    EXPECT_EQ(edit::pickVisibility("team", {}), std::nullopt);
}
