#include <gtest/gtest.h>

#include "core/teleport/TeleportRules.h"

using namespace bs::core::teleport;

TEST(Teleport, TransportModes) {
    EXPECT_EQ(transportModeFromString("same_dimension", TransportMode::Disabled), TransportMode::SameDimension);
    EXPECT_EQ(transportModeFromString("ENABLED", TransportMode::Disabled), TransportMode::Enabled);
    EXPECT_EQ(transportModeFromString("bogus", TransportMode::Enabled), TransportMode::Enabled);
    EXPECT_EQ(toString(TransportMode::SameDimension), "SAME_DIMENSION");
    EXPECT_TRUE(allowsTransport(TransportMode::Enabled, true));
    EXPECT_TRUE(allowsTransport(TransportMode::SameDimension, false));
    EXPECT_FALSE(allowsTransport(TransportMode::SameDimension, true));
    EXPECT_FALSE(allowsTransport(TransportMode::Disabled, false));
}

TEST(Teleport, LandingOrderPrefersFacing) {
    EXPECT_EQ(landingOrder(kNorth), (std::vector<int>{kNorth, kEast, kWest, kSouth, kNorth}));
    auto l = findLanding(kNorth, false, [](int, int) { return true; });
    EXPECT_EQ(l.dx, 0);
    EXPECT_EQ(l.dz, -1);
    EXPECT_EQ(l.facing, kNorth);
    EXPECT_FLOAT_EQ(yawOf(kNorth), 180.f);
    EXPECT_FLOAT_EQ(yawOf(kEast), 270.f);
}

TEST(Teleport, LandingFallsBackEastWestSouthNorth) {
    // facing south blocked, east blocked -> west
    auto l = findLanding(kSouth, false, [](int dx, int dz) { return !(dx == 0 && dz == 1) && dx != 1; });
    EXPECT_EQ(l.dx, -1);
    EXPECT_EQ(l.dz, 0);
    EXPECT_EQ(l.facing, kWest);
    // everything blocked -> the waystone itself, looking along its facing
    auto none = findLanding(kEast, false, [](int, int) { return false; });
    EXPECT_EQ(none.dx, 0);
    EXPECT_EQ(none.dz, 0);
    EXPECT_EQ(none.facing, kEast);
    // warp plates: on the block
    auto plate = findLanding(kWest, true, [](int, int) { return true; });
    EXPECT_EQ(plate.dx, 0);
    EXPECT_EQ(plate.dz, 0);
}

TEST(Teleport, LeashRulesInWaystonesOrder) {
    EXPECT_FALSE(checkLeashed(TransportMode::Disabled, 0, true, true));
    EXPECT_EQ(checkLeashed(TransportMode::Disabled, 1, true, true), LeashError::Disabled);
    EXPECT_EQ(checkLeashed(TransportMode::SameDimension, 1, true, true), LeashError::DeniedEntity);
    EXPECT_EQ(checkLeashed(TransportMode::SameDimension, 2, false, true), LeashError::DisabledDimension);
    EXPECT_FALSE(checkLeashed(TransportMode::SameDimension, 2, false, false));
    EXPECT_FALSE(checkLeashed(TransportMode::Enabled, 2, false, true));
    EXPECT_EQ(message(LeashError::DisabledDimension).second, "You cannot take leashed mobs with you between worlds");
}

TEST(Teleport, DenyList) {
    std::vector<std::string> deny{"minecraft:wither", "ender_dragon"};
    EXPECT_TRUE(isDenied(deny, "minecraft:wither"));
    EXPECT_TRUE(isDenied(deny, "minecraft:ender_dragon"));
    EXPECT_FALSE(isDenied(deny, "minecraft:wolf"));
}

TEST(Teleport, InteractionRange) {
    // eye 8 blocks away horizontally from the block's face: within 4.5 + 4
    EXPECT_TRUE(canInteractWithBlock(9.0, 1.6, 0.5, 0, 0, 0, 4.5, 4.0));
    EXPECT_FALSE(canInteractWithBlock(10.0, 1.6, 0.5, 0, 0, 0, 4.5, 4.0));
    EXPECT_TRUE(canInteractWithBlock(0.5, 0.5, 0.5, 0, 0, 0, 4.5, 4.0));
    EXPECT_TRUE(entityNearBlock(8.5, 0.5, 0.5, 0, 0, 0));
    EXPECT_FALSE(entityNearBlock(8.6, 0.5, 0.5, 0, 0, 0));
}

TEST(Teleport, TargetBlocks) {
    EXPECT_EQ(expectedBlockPath("waystone", "mossy_waystone"), "mossy_waystone");
    EXPECT_EQ(expectedBlockPath("waystone", ""), "waystone");
    EXPECT_EQ(expectedBlockPath("red_sharestone", "red_sharestone"), "red_sharestone");
    EXPECT_EQ(expectedBlockPath("warp_plate", ""), "warp_plate");
    EXPECT_TRUE(isTeleportTargetPath("waystone"));
    EXPECT_TRUE(isTeleportTargetPath("red_nether_bricks_waystone"));
    EXPECT_TRUE(isTeleportTargetPath("blue_sharestone"));
    EXPECT_TRUE(isTeleportTargetPath("warp_plate"));
    EXPECT_TRUE(isTeleportTargetPath("fleeting_memorial"));
    EXPECT_FALSE(isTeleportTargetPath("white_portstone"));
    EXPECT_FALSE(isTeleportTargetPath("stone"));
}
