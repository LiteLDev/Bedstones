#include <gtest/gtest.h>

#include <set>
#include <tuple>

#include "core/portal/Placement.h"

using namespace bs::core::portal;

namespace {
// A flat world: solid below `ground`, air above, plus explicit extra solid cells.
struct World {
    std::set<std::tuple<int, int, int>> blocks;
    int                                 ground = 0;
    bool solid(int x, int y, int z) const { return y < ground || blocks.contains({x, y, z}); }
    bool free(int x, int y, int z) const { return !solid(x, y, z); }
};

std::optional<Cell> spotIn(World const& w, double py = 0.0, double lookX = 1.0, double lookZ = 0.0) {
    return findPortalSpot(
        0.5, py, 0.5, lookX, lookZ,
        [&](int x, int y, int z) { return w.solid(x, y, z); },
        [&](int x, int y, int z) { return w.free(x, y, z); }
    );
}
} // namespace

TEST(Portal, OpensThreeBlocksAheadOnFlatGround) {
    World w;
    auto  spot = spotIn(w);
    ASSERT_TRUE(spot.has_value());
    EXPECT_EQ(*spot, (Cell{3, 0, 0}));
}

TEST(Portal, FollowsTheHorizontalLookDirection) {
    World w;
    auto  north = spotIn(w, 0.0, 0.0, -2.0);
    ASSERT_TRUE(north);
    EXPECT_EQ(*north, (Cell{0, 0, -3}));
    EXPECT_FALSE(spotIn(w, 0.0, 0.0, 0.0)) << "looking straight up/down";
}

TEST(Portal, ScansFromTwoAboveToThreeBelow) {
    World w;
    w.ground = -3; // lowest reachable spot
    auto spot = spotIn(w);
    ASSERT_TRUE(spot);
    EXPECT_EQ(spot->y, -3);
    World deep;
    deep.ground = -4;
    EXPECT_FALSE(spotIn(deep));
    // A step up: a 2 high wall three ahead -> portal on top of it.
    World step;
    step.blocks = {{3, 0, 0}, {3, 1, 0}};
    auto up     = spotIn(step);
    ASSERT_TRUE(up);
    EXPECT_EQ(*up, (Cell{3, 2, 0}));
}

TEST(Portal, FallsBackToFourAheadAndFailsWithoutRoom) {
    World w;
    for (int y = -4; y <= 4; ++y) w.blocks.insert({3, y, 0}); // column 3 fully blocked
    auto spot = spotIn(w);
    ASSERT_TRUE(spot);
    EXPECT_EQ(*spot, (Cell{4, 0, 0}));
    EXPECT_FALSE(spotIn(w, 50.0)) << "in the air: nothing solid below within reach";
    World low; // only 2 free cells above the ground
    for (int x = -10; x <= 10; ++x) low.blocks.insert({x, 2, 0});
    EXPECT_FALSE(spotIn(low));
}

TEST(Memorial, AboveTheDeathCellWhenFree) {
    auto spot = findMemorialSpot({5, 10, 5}, [](int, int, int) { return true; });
    ASSERT_TRUE(spot);
    EXPECT_EQ(*spot, (Cell{5, 11, 5}));
}

TEST(Memorial, SearchesTheSquaresAroundTheDeathCell) {
    // Only the cell above is blocked: first cell of the radius 1 square at the death height.
    auto first = findMemorialSpot({0, 0, 0}, [](int x, int y, int z) { return !(x == 0 && y == 1 && z == 0); });
    ASSERT_TRUE(first);
    EXPECT_EQ(*first, (Cell{-1, 0, -1}));
    // Only (7, 13, 5) is free: radius 2, dy 3.
    auto spot = findMemorialSpot({5, 10, 5}, [](int x, int y, int z) { return x == 7 && y == 13 && z == 5; });
    ASSERT_TRUE(spot);
    EXPECT_EQ(*spot, (Cell{7, 13, 5}));
    // Radius 1 wins over radius 2 at the same height.
    auto near = findMemorialSpot({0, 0, 0}, [](int x, int y, int z) { return y == 1 && (x == 1 || x == 2) && z == 0; });
    ASSERT_TRUE(near);
    EXPECT_EQ(*near, (Cell{1, 1, 0}));
    // Nothing free within reach (dy 4 is out of range).
    EXPECT_FALSE(findMemorialSpot({0, 0, 0}, [](int, int y, int) { return y == 4; }));
}
