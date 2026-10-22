#include <gtest/gtest.h>

#include <cmath>
#include <set>

#include "core/visuals/Visuals.h"

using namespace bs::core;
using namespace bs::core::visuals;

namespace {
WaystoneRecord rec(std::string uid, std::string type, std::string style, GlobalPos pos) {
    WaystoneRecord r;
    r.uid   = std::move(uid);
    r.type  = std::move(type);
    r.style = std::move(style);
    r.pos   = pos;
    return r;
}
} // namespace

TEST(Visuals, RuneColors) {
    EXPECT_EQ(waystoneRuneColor("waystone"), 0xFFFFFFu);
    EXPECT_EQ(waystoneRuneColor("mossy_waystone"), 0xFFFFFFu);
    EXPECT_EQ(waystoneRuneColor("blackstone_waystone"), 0x993333u);
    EXPECT_EQ(waystoneRuneColor("end_stone_waystone"), 0x7200FFu);
    EXPECT_EQ(dyeRuneColor("red_sharestone"), 0xB02E26u);
    EXPECT_EQ(dyeRuneColor("light_blue_portstone"), 0x3AB3DAu);
    EXPECT_EQ(dyeRuneColor("white_portstone"), 0xF9FFFEu);
    EXPECT_EQ(dyeRuneColor("black"), 0x1D1D21u);
    EXPECT_EQ(dyeRuneColor("nonsense"), 0xFFFFFFu);
    Entry e{"a", {}, "red_sharestone", "red_sharestone", 0, false};
    EXPECT_EQ(runeColor(e), 0xB02E26u);
    e = {"b", {}, "waystone", "end_stone_waystone", 0, true};
    EXPECT_EQ(runeColor(e), 0x7200FFu);
}

TEST(Visuals, WhatIsShown) {
    Entry ws{"a", {}, "waystone", "waystone", 0, false};
    EXPECT_FALSE(showsRunes(ws));
    EXPECT_FALSE(hasAmbientParticles(ws));
    EXPECT_FALSE(hasFloatingItem(ws));
    ws.activated = true;
    EXPECT_TRUE(showsRunes(ws));
    EXPECT_TRUE(hasAmbientParticles(ws));
    Entry ss{"b", {}, "blue_sharestone", "blue_sharestone", 0, false};
    EXPECT_TRUE(showsRunes(ss));
    EXPECT_FALSE(hasAmbientParticles(ss));
    EXPECT_TRUE(hasFloatingItem(ss));
    EXPECT_TRUE(isRendered("waystone"));
    EXPECT_TRUE(isRendered("red_sharestone"));
    EXPECT_FALSE(isRendered("warp_plate"));
    EXPECT_FALSE(isRendered("warp_portal"));
}

TEST(Visuals, SelectFiltersByRangeTypeAndDimension) {
    auto near   = rec("near", "waystone", "waystone", {0, 10, 64, 10});
    auto far    = rec("far", "waystone", "waystone", {0, 500, 64, 0});
    auto nether = rec("nether", "waystone", "waystone", {1, 0, 64, 0});
    auto plate  = rec("plate", "warp_plate", "warp_plate", {0, 1, 64, 1});
    auto share  = rec("share", "red_sharestone", "red_sharestone", {0, -20, 70, 5});
    auto trans  = rec("trans", "waystone", "waystone", {0, 2, 64, 2});
    trans.transient = true;
    std::vector<WaystoneRecord const*> all{&near, &far, &nether, &plate, &share, &trans, nullptr};
    std::set<std::string>              activated{"near", "share"};
    auto out = select(all, 0, 0.5, 64, 0.5, kSyncRange, [&](std::string const& uid) { return activated.contains(uid); });
    ASSERT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0].uid, "near");
    EXPECT_TRUE(out[0].activated);
    EXPECT_EQ(out[1].uid, "share");
    EXPECT_FALSE(out[1].activated) << "only waystones carry an activation flag";
}

TEST(Visuals, RangeEdge) {
    EXPECT_TRUE(inRange({0, 127, 0, 0}, 0, 0.5, 0.5, 0.5, 128));
    EXPECT_FALSE(inRange({0, 129, 0, 0}, 0, 0.5, 0.5, 0.5, 128));
    EXPECT_FALSE(inRange({2, 0, 0, 0}, 0, 0.5, 0.5, 0.5, 128));
}

TEST(Visuals, JsonRoundTrip) {
    std::vector<Entry> in{
        {"u1", {0, 1, 2, 3},    "waystone",       "mossy_waystone", 2, true },
        {"u2", {1, -5, 70, 9}, "lime_sharestone", "lime_sharestone", 3, false},
    };
    auto back = fromJson(nlohmann::json::parse(toJson(in).dump()));
    EXPECT_EQ(back, in);
    EXPECT_TRUE(fromJson(nlohmann::json::array()).empty());
    auto bad = nlohmann::json{
        {"entries", {{{"uid", "x"}, {"type", "warp_plate"}}, {{"uid", 5}}, {{"uid", "ok"}, {"type", "waystone"}}}}
    };
    auto parsed = fromJson(bad);
    ASSERT_EQ(parsed.size(), 1u);
    EXPECT_EQ(parsed[0].uid, "ok");
}

TEST(Visuals, RuneQuadsSurroundTheColumn) {
    auto quads = runeQuads({0, 10, 64, -3});
    // south, west, north, east
    EXPECT_DOUBLE_EQ(quads[0].z, -3 + 0.5 + kRuneInset);
    EXPECT_DOUBLE_EQ(quads[1].x, 10 + 0.5 - kRuneInset);
    EXPECT_DOUBLE_EQ(quads[2].z, -3 + 0.5 - kRuneInset);
    EXPECT_DOUBLE_EQ(quads[3].x, 10 + 0.5 + kRuneInset);
    for (auto const& q : quads) {
        EXPECT_DOUBLE_EQ(q.y, 64 + kRuneCentreY);
        EXPECT_DOUBLE_EQ(std::hypot(q.nx, q.nz), 1.0);
    }
}

TEST(Visuals, FloatingPose) {
    auto p = floatingPose(0, 0);
    EXPECT_DOUBLE_EQ(p.angleDeg, 0);
    EXPECT_DOUBLE_EQ(p.bob, 0);
    p = floatingPose(720 + 100, 0.5);
    EXPECT_DOUBLE_EQ(p.angleDeg, 50.25);
    EXPECT_NEAR(p.bob, std::sin(820.5 / 8) * 0.025, 1e-12);
    EXPECT_LE(std::abs(floatingPose(12345, 0.3).bob), 0.025);
}
