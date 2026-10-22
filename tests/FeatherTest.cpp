#include <gtest/gtest.h>

#include "core/feather/Feather.h"

using namespace bs::core::feather;

TEST(Feather, RayHitsPlayerInFront) {
    // Eye at 1.62 above feet, looking +z at a player 1.5 blocks away.
    Vec eye{0.5, 65.62, 0.5};
    auto hit = rayHitPlayer(eye, {0, 0, 1}, {0.5, 64, 2.0});
    ASSERT_TRUE(hit.has_value());
    EXPECT_NEAR(*hit, 1.5 - 0.3 - kPickInflate, 1e-9);
}

TEST(Feather, RayMissesBeyondRangeOrBehind) {
    Vec eye{0.5, 65.62, 0.5};
    EXPECT_FALSE(rayHitPlayer(eye, {0, 0, 1}, {0.5, 64, 3.5}).has_value());  // 3 blocks away
    EXPECT_FALSE(rayHitPlayer(eye, {0, 0, -1}, {0.5, 64, 2.0}).has_value()); // looking away
    EXPECT_FALSE(rayHitPlayer(eye, {1, 0, 0}, {0.5, 64, 2.0}).has_value());  // looking sideways
    EXPECT_FALSE(rayHitPlayer(eye, {0, 0, 0}, {0.5, 64, 2.0}).has_value());  // no direction
}

TEST(Feather, RayHitsSlightlyOffCentre) {
    Vec eye{0.5, 65.62, 0.5};
    // Looking a bit down and to the side still touches the inflated box.
    EXPECT_TRUE(rayHitPlayer(eye, {0.2, -0.3, 1}, {0.5, 64, 1.8}).has_value());
}

TEST(Feather, PickTargetTakesTheClosest) {
    Vec eye{0.5, 65.62, 0.5};
    std::vector<Vec> feet{{0.5, 64, 2.4}, {0.5, 64, 1.6}, {5, 64, 5}};
    EXPECT_EQ(pickTarget(eye, {0, 0, 1}, feet), 1);
    EXPECT_EQ(pickTarget(eye, {0, 0, -1}, feet), -1);
}

TEST(Feather, HeartPosIsTheMidpoint) {
    auto h = heartPos({0, 64, 0}, {2, 64, 4});
    EXPECT_DOUBLE_EQ(h.x, 1);
    EXPECT_DOUBLE_EQ(h.y, 65.1);
    EXPECT_DOUBLE_EQ(h.z, 2);
}

TEST(Feather, LinkSets) {
    EXPECT_TRUE(parseLinks("").empty());
    EXPECT_EQ(parseLinks("a, b,,a"), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(withLink("", "a"), "a");
    EXPECT_EQ(withLink("a", "b"), "a,b");
    EXPECT_EQ(withLink("a,b", "a"), "a,b");
    EXPECT_TRUE(hasLink("a,b", "b"));
    EXPECT_FALSE(hasLink("a,b", "c"));
    EXPECT_FALSE(hasLink("a,b", ""));
}

TEST(Feather, MutualLinks) {
    FeatherInfo a{"A", "B"}, b{"B", "A"}, c{"C", "A"};
    EXPECT_TRUE(areLinked(a, b));
    EXPECT_FALSE(areLinked(a, c)); // one-sided
    EXPECT_FALSE(areLinked(FeatherInfo{"", "B"}, b));
    EXPECT_TRUE(anyLinked({c, a}, {b}));
    EXPECT_FALSE(anyLinked({c}, {b}));
}

TEST(Feather, TwinningTracker) {
    TwinningTracker t;
    EXPECT_EQ(t.advance("p", "q"), 1);
    EXPECT_EQ(t.advance("p", "q"), 2);
    EXPECT_EQ(t.advance("p", "r"), 1); // target changed
    for (int i = 0; i < kTwinningDuration - 1; ++i) t.advance("p", "r");
    EXPECT_EQ(t.ticks("p"), kTwinningDuration);
    t.advance("x", "p");
    t.retain({"x"});
    EXPECT_EQ(t.ticks("p"), 0);
    EXPECT_EQ(t.ticks("x"), 1);
    t.reset("x");
    EXPECT_TRUE(t.empty());
}
