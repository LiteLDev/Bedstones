#include <gtest/gtest.h>

#include "core/commands/AdminLogic.h"

using namespace bs::core;
using namespace bs::core::commands;

TEST(Commands, CooldownIdNamespace) {
    EXPECT_EQ(normalizeCooldownId("inventory_button"), "waystones:inventory_button");
    EXPECT_EQ(normalizeCooldownId("Waystones:Warp_Stone"), "waystones:warp_stone");
    EXPECT_EQ(normalizeCooldownId("mymod:x"), "mymod:x");
    EXPECT_EQ(normalizeCooldownId(""), "");
}

TEST(Commands, CooldownMaths) {
    EXPECT_EQ(cooldownSet(1000, 5), 6000);
    EXPECT_EQ(cooldownSet(1000, 0), 1000);
    // add extends from now when the cooldown has expired / is absent
    EXPECT_EQ(cooldownAdd(1000, std::nullopt, 10), 11000);
    EXPECT_EQ(cooldownAdd(1000, 500, 10), 11000);
    // ... and from the expiry while it is running
    EXPECT_EQ(cooldownAdd(1000, 5000, 10), 15000);
    EXPECT_EQ(cooldownAdd(1000, 5000, -2), 3000);
}

TEST(Commands, DimensionNames) {
    EXPECT_EQ(dimensionName(0), "minecraft:overworld");
    EXPECT_EQ(dimensionName(1), "minecraft:the_nether");
    EXPECT_EQ(dimensionName(2), "minecraft:the_end");
}

TEST(Commands, Distance) {
    EXPECT_EQ(blockDistance(0.5, 0.5, 0.5, GlobalPos{0, 0, 0, 0}), 0);
    EXPECT_EQ(blockDistance(0.5, 0.5, 10.4, GlobalPos{0, 0, 0, 0}), 9);
    EXPECT_EQ(blockDistance(3.5, 4.5, 0.5, GlobalPos{0, 0, 0, 0}), 5);
}

TEST(Commands, AdminInspectionOrder) {
    std::vector<ListEntry> e{
        {"far", "Far", {0, 100, 0, 0}, false},
        {"nether", "Nether", {1, 1, 0, 0}, false},
        {"near", "Near", {0, 5, 0, 0}, false},
        {"ownedFar", "OwnedFar", {0, 200, 0, 0}, true},
        {"ownedNear", "OwnedNear", {0, 2, 0, 0}, true},
    };
    sortForAdminInspection(e, Viewpoint{0, 0.5, 0.5, 0.5});
    std::vector<std::string> order;
    for (auto const& x : e) order.push_back(x.uid);
    EXPECT_EQ(order, (std::vector<std::string>{"ownedNear", "ownedFar", "near", "far", "nether"}));

    // console: owned first, otherwise the stored order is kept
    std::vector<ListEntry> c{{"a", "", {}, false}, {"b", "", {}, true}, {"c", "", {}, false}};
    sortForAdminInspection(c, std::nullopt);
    EXPECT_EQ(c[0].uid, "b");
    EXPECT_EQ(c[1].uid, "a");
    EXPECT_EQ(c[2].uid, "c");
}

TEST(Commands, FeatherLinks) {
    EXPECT_TRUE(feathersLinked("a", "b", "b", "a"));
    EXPECT_FALSE(feathersLinked("a", "c", "b", "a"));
    EXPECT_FALSE(feathersLinked(std::nullopt, "b", "b", std::nullopt));
    EXPECT_FALSE(feathersLinked("a", std::nullopt, "b", std::nullopt));
}
