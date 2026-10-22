#include <gtest/gtest.h>

#include <set>
#include <tuple>

#include "core/portal/Placement.h"

using namespace bs::core::portal;

namespace {
// A flat world: solid at y < 0, air above, plus explicit extra solid cells.
struct World {
    std::set<std::tuple<int, int, int>> blocks;
    int                                 ground = 0;
    bool solid(int x, int y, int z) const { return y < ground || blocks.contains({x, y, z}); }
    bool free(int x, int y, int z) const { return !solid(x, y, z); }
};
} // namespace

TEST(Portal, OpensThreeBlocksAheadOnFlatGround) {
    World w;
    auto  spot = findPortalSpot(
        0.5, 0.0, 0.5, 1.0, 0.0,
        [&](int x, int y, int z) { return w.solid(x, y, z); },
        [&](int x, int y, int z) { return w.free(x, y, z); }
    );
    ASSERT_TRUE(spot.has_value());
    EXPECT_EQ(*spot, (Cell{3, 0, 0}));
}

TEST(Portal, FollowsTheHorizontalLookDirection) {
    World w;
    auto  solid = [&](int x, int y, int z) { return w.solid(x, y, z); };
    auto  free  = [&](int x, int y, int z) { return w.free(x, y, z); };
    auto  north = findPortalSpot(0.5, 0.0, 0.5, 0.0, -2.0, solid, free);
    ASSERT_TRUE(north);
    EXPECT_EQ(*north, (Cell{0, 0, -3}));
    EXPECT_FALSE(findPortalSpot(0.5, 0.0, 0.5, 0.0, 0.0, solid, free)) << "looking straight up/down";
}

TEST(Portal, ScansDownAndUpWithinTwoBlocks) {
    World w;
    w.ground = -2; // the ground is two blocks below the player
    auto spot = findPortalSpot(
        0.5, 0.0, 0.5, 1.0, 0.0,
        [&](int x, int y, int z) { return w.solid(x, y, z); },
        [&](int x, int y, int z) { return w.free(x, y, z); }
    );
    ASSERT_TRUE(spot);
    EXPECT_EQ(spot->y, -2);
    // A step up: a 2 high block wall three ahead -> portal on top of it.
    World step;
    step.blocks = {{3, 0, 0}, {3, 1, 0}};
    auto up     = findPortalSpot(
        0.5, 0.0, 0.5, 1.0, 0.0,
        [&](int x, int y, int z) { return step.solid(x, y, z); },
        [&](int x, int y, int z) { return step.free(x, y, z); }
    );
    ASSERT_TRUE(up);
    EXPECT_EQ(*up, (Cell{3, 2, 0}));
}

TEST(Portal, FallsBackToFourAheadAndFailsWithoutRoom) {
    World w;
    // Column 3 is fully blocked from y -3 to 4: the 4th column is used.
    for (int y = -3; y <= 4; ++y) w.blocks.insert({3, y, 0});
    auto solid = [&](int x, int y, int z) { return w.solid(x, y, z); };
    auto free  = [&](int x, int y, int z) { return w.free(x, y, z); };
    auto spot  = findPortalSpot(0.5, 0.0, 0.5, 1.0, 0.0, solid, free);
    ASSERT_TRUE(spot);
    EXPECT_EQ(*spot, (Cell{4, 0, 0}));
    // In the air: nothing solid below within reach.
    EXPECT_FALSE(findPortalSpot(0.5, 50.0, 0.5, 1.0, 0.0, solid, free));
    // Low ceiling: only 2 free cells above the ground.
    World low;
    for (int x = -10; x <= 10; ++x) low.blocks.insert({x, 2, 0});
    EXPECT_FALSE(findPortalSpot(
        0.5, 0.0, 0.5, 1.0, 0.0,
        [&](int x, int y, int z) { return low.solid(x, y, z); },
        [&](int x, int y, int z) { return low.free(x, y, z); }
    ));
}

TEST(Memorial, AboveTheDeathCellWhenFree) {
    auto spot = findMemorialSpot({5, 10, 5}, [](int, int, int) { return true; });
    ASSERT_TRUE(spot);
    EXPECT_EQ(*spot, (Cell{5, 11, 5}));
}

TEST(Memorial, SearchesTheRingsAroundIt) {
    // Only (7, 13, 5) is free: radius 2, dy 2.
    auto spot = findMemorialSpot({5, 10, 5}, [](int x, int y, int z) { return x == 7 && y == 13 && z == 5; });
    ASSERT_TRUE(spot);
    EXPECT_EQ(*spot, (Cell{7, 13, 5}));
    // Radius 1 wins over radius 2 at the same height.
    auto near = findMemorialSpot({0, 0, 0}, [](int x, int y, int z) { return y == 1 && (x == 1 || x == 2) && z == 0; });
    ASSERT_TRUE(near);
    EXPECT_EQ(near->x, 1);
    // Nothing free within reach (dy 4 is out of range).
    EXPECT_FALSE(findMemorialSpot({0, 0, 0}, [](int, int y, int) { return y == 5; }));
}
