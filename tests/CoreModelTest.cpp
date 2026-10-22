#include <gtest/gtest.h>

#include "core/Format.h"
#include "core/Warp.h"
#include "core/Waystone.h"

using namespace bs::core;

TEST(Waystone, RecordRoundTrip) {
    WaystoneRecord r;
    r.uid        = randomUuid();
    r.type       = types::sharestone("red");
    r.name       = "Spawn";
    r.pos        = {1, 10, 64, -3};
    r.style      = "red_sharestone";
    r.facing     = 2;
    r.transient  = true;
    r.origin     = Origin::Village;
    r.ownerUid   = "owner";
    r.ownerName  = "Steve";
    r.visibility = Visibility::Sharestone;
    r.extra      = {{"modifiers", {{"count", 2}}}};
    auto back    = WaystoneRecord::fromJson(nlohmann::json::parse(r.toJson().dump()));
    EXPECT_EQ(back.uid, r.uid);
    EXPECT_EQ(back.type, "red_sharestone");
    EXPECT_TRUE(back.isSharestone());
    EXPECT_EQ(back.pos, r.pos);
    EXPECT_EQ(back.facing, 2);
    EXPECT_TRUE(back.transient);
    EXPECT_EQ(back.origin, Origin::Village);
    EXPECT_EQ(back.visibility, Visibility::Sharestone);
    EXPECT_EQ(back.extra["modifiers"]["count"], 2);
}

TEST(Waystone, BadJsonGivesDefaults) {
    auto r = WaystoneRecord::fromJson(nlohmann::json{{"facing", "x"}, {"visibility", "bogus"}});
    EXPECT_EQ(r.type, "waystone");
    EXPECT_EQ(r.facing, 0);
    EXPECT_EQ(r.visibility, Visibility::Activation);
    EXPECT_EQ(WaystoneRecord::fromJson(nlohmann::json::array()).uid, "");
}

TEST(Waystone, SharestoneTypes) {
    EXPECT_EQ(types::sharestoneColor("light_blue_sharestone").value_or(""), "light_blue");
    EXPECT_FALSE(types::sharestoneColor("white_sharestone")); // no white sharestone
    EXPECT_FALSE(types::sharestoneColor("waystone"));
    EXPECT_FALSE(types::isSharestone("warp_plate"));
}

TEST(Waystone, PlayerDataActivation) {
    PlayerWaystoneData d;
    EXPECT_TRUE(d.activate("a"));
    EXPECT_FALSE(d.activate("a"));
    EXPECT_TRUE(d.activate("b"));
    EXPECT_EQ(d.sortingIndex, (std::vector<std::string>{"a", "b"}));
    d.aliases["a"] = "Home";
    EXPECT_EQ(d.effectiveName("a", "Spawn"), "Home");
    EXPECT_EQ(d.effectiveName("b", "Spawn"), "Spawn");
    EXPECT_TRUE(d.forget("a"));
    EXPECT_FALSE(d.isActivated("a"));
    EXPECT_EQ(d.sortingIndex, (std::vector<std::string>{"b"}));
}

TEST(Waystone, PlayerDataRoundTripKeepsExtra) {
    PlayerWaystoneData d;
    d.activate("x");
    d.sortMode         = "distance";
    d.cooldowns["cd"]  = 1234;
    d.groups["x"]      = {"waystones:favorites"};
    d.hidden.insert("y");
    d.groupRegistry.push_back({"waystones:user/1", "Mine", "players", "gold", false, false, 3});
    d.extra["feather"] = "abc";
    auto back          = PlayerWaystoneData::fromJson(nlohmann::json::parse(d.toJson().dump()));
    EXPECT_TRUE(back.isActivated("x"));
    EXPECT_EQ(back.sortMode, "distance");
    EXPECT_EQ(back.cooldowns.at("cd"), 1234);
    EXPECT_EQ(back.groups.at("x").front(), "waystones:favorites");
    EXPECT_TRUE(back.hidden.contains("y"));
    ASSERT_EQ(back.groupRegistry.size(), 1u);
    EXPECT_EQ(back.groupRegistry[0].color, "gold");
    EXPECT_EQ(back.extra["feather"], "abc");
}

TEST(Waystone, RandomUuidShape) {
    auto a = randomUuid(), b = randomUuid();
    EXPECT_EQ(a.size(), 36u);
    EXPECT_EQ(a[14], '4');
    EXPECT_NE(a, b);
}

TEST(Waystone, Glob) {
    EXPECT_TRUE(globMatch("*", "anything"));
    EXPECT_TRUE(globMatch("core.*", "core.storage"));
    EXPECT_FALSE(globMatch("core.*", "selection.list"));
    EXPECT_TRUE(globMatch("a?c", "abc"));
}

TEST(Warp, SourceAndTargetRoundTrip) {
    WarpSource s{WarpSourceKind::Portstone, "uid", {0, 1, 2, 3}, "", "red", 1};
    auto       s2 = WarpSource::fromJson(s.toJson());
    EXPECT_EQ(s2.kind, WarpSourceKind::Portstone);
    EXPECT_TRUE(s2.isBlock());
    EXPECT_EQ(s2.portstoneColor, "red");
    WarpTarget t;
    t.id                      = "u";
    t.kind                    = "waystone";
    t.requirement.xpPoints    = 12;
    t.requirement.cooldowns   = {{"waystones:inventory_button", 300, false}};
    t.requirement.items       = {{"minecraft:diamond", 2}};
    auto t2                   = WarpTarget::fromJson(nlohmann::json::parse(t.toJson().dump()));
    EXPECT_EQ(t2.requirement.xpPoints, 12);
    EXPECT_EQ(t2.requirement.cooldowns.at(0).seconds, 300);
    EXPECT_EQ(t2.requirement.items.at(0).count, 2);
    EXPECT_FALSE(t2.requirement.free());
}

TEST(Warp, XpMaths) {
    EXPECT_EQ(xpForLevel(0), 7);
    EXPECT_EQ(xpForLevel(15), 37);
    EXPECT_EQ(xpForLevel(30), 112);
    EXPECT_EQ(totalXpForLevel(30), 1395);
    EXPECT_EQ(levelsForPoints(30, 0.f, 0), 0);
    EXPECT_EQ(levelsForPoints(30, 0.f, 1), 1);
    EXPECT_EQ(levelsForPoints(5, 0.f, 27), 2); // total 55 -> 28 points left = level 3
}

TEST(Format, Placeholders) {
    EXPECT_EQ(formatMessageArgs("Activated Waystone: {}", "Home"), "Activated Waystone: Home");
    EXPECT_EQ(formatMessageArgs("{1} {0}", "a", "b"), "b a");
}
