#include <gtest/gtest.h>

#include "core/selection/Selection.h"

using namespace bs::core;
using namespace bs::core::selection;

namespace {
WarpTarget target(std::string id, std::string name, GlobalPos pos, std::string kind = "waystone") {
    WarpTarget t;
    t.id   = std::move(id);
    t.name = std::move(name);
    t.pos  = pos;
    t.kind = std::move(kind);
    return t;
}
std::vector<std::string> ids(std::vector<WarpTarget> const& v) {
    std::vector<std::string> out;
    for (auto const& t : v) out.push_back(t.id);
    return out;
}
} // namespace

TEST(Selection, SortModeCycle) {
    EXPECT_EQ(nextSortMode(SortMode::Manual), SortMode::Name);
    EXPECT_EQ(nextSortMode(SortMode::Name), SortMode::Distance);
    EXPECT_EQ(nextSortMode(SortMode::Distance), SortMode::Manual);
    for (auto m : {SortMode::Manual, SortMode::Name, SortMode::Distance}) EXPECT_EQ(sortModeFromString(toString(m)), m);
    EXPECT_FALSE(sortModeFromString("bogus").has_value());
}

TEST(Selection, SortByNameIsCaseInsensitive) {
    std::vector<WarpTarget> v{target("c", "charlie", {}), target("a", "Alpha", {}), target("b", "bravo", {})};
    sortTargets(v, SortMode::Name, {}, {});
    EXPECT_EQ(ids(v), (std::vector<std::string>{"a", "b", "c"}));
}

TEST(Selection, SortByDistanceSameDimensionFirst) {
    std::vector<WarpTarget> v{
        target("nether", "N", {1, 1, 64, 1}),
        target("far", "F", {0, 1000, 64, 0}),
        target("end", "E", {2, 0, 64, 0}),
        target("near", "A", {0, 10, 64, 0}),
    };
    sortTargets(v, SortMode::Distance, {}, {0, 0, 64, 0});
    EXPECT_EQ(ids(v), (std::vector<std::string>{"near", "far", "nether", "end"}));
}

TEST(Selection, SortManualUsesSortingIndex) {
    std::vector<WarpTarget> v{target("a", "A", {}), target("b", "B", {}), target("x", "X", {}), target("c", "C", {})};
    sortTargets(v, SortMode::Manual, {"c", "a", "b"}, {});
    EXPECT_EQ(ids(v), (std::vector<std::string>{"c", "a", "b", "x"}));
}

TEST(Selection, PinnedFirst) {
    std::vector<WarpTarget> v{
        target("a", "A", {}),
        target("portal", "Z portal", {}, "warp_portal"),
        target("b", "B", {}),
        target("memorial", "Y memorial", {}, "fleeting_memorial"),
    };
    v[2].pinned = true;
    sortTargets(v, SortMode::Name, {}, {});
    EXPECT_EQ(ids(v), (std::vector<std::string>{"portal", "b", "memorial", "a"}));
}

TEST(Selection, Search) {
    EXPECT_TRUE(matchesSearch("Iron Mine", ""));
    EXPECT_TRUE(matchesSearch("Iron Mine", "mINe"));
    EXPECT_FALSE(matchesSearch("Iron Mine", "gold"));
}

TEST(Selection, DistanceFormat) {
    EXPECT_EQ(formatDistance(123.7), "123m");
    EXPECT_EQ(formatDistance(9999), "9999m");
    EXPECT_EQ(formatDistance(10000), "10.0km");
    EXPECT_EQ(formatDistance(15240), "15.2km");
    EXPECT_EQ(formatDistance(1234, true), "1.2km");
    EXPECT_NEAR(distanceTo({0, 0.5, 0.5, 0.5}, {0, 3, 0, 4}), 5.0, 1e-9);
}

TEST(Selection, CooldownFormat) {
    EXPECT_EQ(formatCooldown(0), "00:00");
    EXPECT_EQ(formatCooldown(59.2), "01:00");
    EXPECT_EQ(formatCooldown(125), "02:05");
    EXPECT_EQ(formatCooldown(300), "05:00");
}

TEST(Selection, SelectableAndLevels) {
    WarpTarget t;
    EXPECT_TRUE(isSelectable(t, false));
    t.canAfford = false;
    EXPECT_FALSE(isSelectable(t, false));
    EXPECT_TRUE(isSelectable(t, true));
    t.current = true;
    EXPECT_FALSE(isSelectable(t, true));
    EXPECT_EQ(levelSprite(0).sprite, 0);
    EXPECT_EQ(levelSprite(2).sprite, 2);
    EXPECT_FALSE(levelSprite(3).plus);
    EXPECT_EQ(levelSprite(7).sprite, 3);
    EXPECT_TRUE(levelSprite(7).plus);
}

TEST(Selection, LabelColorAndFilters) {
    std::vector<GroupInfo> groups{
        {"waystones:favorites", "Favorites", "favorites", "gold", false},
        {"waystones:global", "Global", "global", "yellow", false},
        {"waystones:hidden", "Hidden", "x", "red", true},
        {"waystones:dimension/0", "Overworld", "dimension", "white", false},
    };
    auto t   = target("a", "A", {});
    t.groups = {"waystones:global", "waystones:favorites"};
    EXPECT_EQ(labelColor(t, groups), "gold");
    t.groups = {"waystones:hidden", "waystones:global"};
    EXPECT_EQ(labelColor(t, groups), "yellow");
    t.groups = {};
    EXPECT_EQ(labelColor(t, groups), "white");
    auto portal = target("p", "P", {}, "warp_portal");
    EXPECT_EQ(labelColor(portal, groups), "light_purple");

    auto u   = target("b", "B", {});
    u.groups = {"waystones:dimension/0", "waystones:hidden"};
    t.groups = {"waystones:global"};
    auto f   = filterGroups(groups, {t, u}, 5);
    ASSERT_EQ(f.size(), 2u);
    EXPECT_EQ(f[0].id, "waystones:global");
    EXPECT_EQ(f[1].id, "waystones:dimension/0");
    EXPECT_EQ(filterGroups(groups, {t, u}, 1).size(), 1u);
}

TEST(Selection, Layout) {
    EXPECT_EQ(panelHeight(0, 300), 200);
    EXPECT_EQ(panelHeight(10, 400), 64 + 220);
    EXPECT_EQ(panelHeight(30, 300), 260);
    EXPECT_EQ(maxGroupButtons(200), 5);
    EXPECT_EQ(maxGroupButtons(244), 7);
}
