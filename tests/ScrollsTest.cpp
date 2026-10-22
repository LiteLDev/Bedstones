#include <gtest/gtest.h>

#include "core/scrolls/Scrolls.h"

using namespace bs::core;
using namespace bs::core::scrolls;

TEST(Scrolls, ItemClassification) {
    EXPECT_EQ(fromPath("warp_scroll"), ScrollItem::WarpScroll);
    EXPECT_EQ(fromPath("bound_scroll"), ScrollItem::BoundScroll);
    EXPECT_FALSE(fromPath("portal_scroll").has_value());
    EXPECT_EQ(path(ScrollItem::WarpStone), "warp_stone");
    EXPECT_FALSE(chargeable(ScrollItem::BlankScroll));
    EXPECT_TRUE(chargeable(ScrollItem::ReturnScroll));
    EXPECT_EQ(sourceKind(ScrollItem::ReturnScroll), WarpSourceKind::ReturnScroll);
    EXPECT_EQ(sourceKind(ScrollItem::WarpStone), WarpSourceKind::WarpStone);
    EXPECT_FALSE(sourceKind(ScrollItem::BlankScroll).has_value());
    EXPECT_TRUE(consumedOnWarp(ScrollItem::BoundScroll));
    EXPECT_FALSE(consumedOnWarp(ScrollItem::WarpStone));
    EXPECT_EQ(useTime(ScrollItem::WarpStone, 40, 32), 40);
    EXPECT_EQ(useTime(ScrollItem::WarpScroll, 40, 32), 32);
    EXPECT_EQ(useTime(ScrollItem::WarpScroll, 40, -3), 0);
}

TEST(Scrolls, ChargeParticlesSchedule) {
    // 32-tick charge: particles every 5 remaining ticks (remaining 30, 25, ..., 0) and a burst at remaining 1.
    EXPECT_FALSE(chargeParticles(ScrollItem::WarpScroll, 1, 32).any());
    auto early = chargeParticles(ScrollItem::WarpScroll, 2, 32); // remaining 30, progress 1/16
    EXPECT_EQ(early.reversePortal, 4);
    EXPECT_EQ(early.crimsonSpore, 0);
    auto mid = chargeParticles(ScrollItem::WarpScroll, 17, 32); // remaining 15, progress ~0.53
    EXPECT_EQ(mid.reversePortal, 25);
    EXPECT_EQ(mid.crimsonSpore, 25);
    EXPECT_EQ(mid.witch, 8);
    auto last = chargeParticles(ScrollItem::WarpScroll, 31, 32);
    EXPECT_GT(last.portal, 0);
    auto stone = chargeParticles(ScrollItem::WarpStone, 27, 32); // remaining 5, progress ~0.84
    EXPECT_EQ(stone.reversePortal, 4 + 40);
    EXPECT_EQ(stone.witch, 13);
    EXPECT_FALSE(chargeParticles(ScrollItem::WarpStone, 33, 32).any());
}

TEST(Scrolls, NearestWaystone) {
    std::vector<Candidate> c{
        {"far", {0, 100, 64, 0}},
        {"near", {0, 10, 64, 0}},
        {"nether", {1, 1, 64, 0}},
    };
    EXPECT_EQ(nearest(c, 0, 0, 64, 0), "near");
    EXPECT_EQ(nearest(c, 1, 0, 64, 0), "nether");
    EXPECT_EQ(nearest(c, 2, 0, 64, 0), "");
    EXPECT_EQ(nearest({}, 0, 0, 0, 0), "");
}

TEST(Scrolls, ReturnTarget) {
    EXPECT_EQ(returnTarget("portal", true, "near"), "portal");
    EXPECT_EQ(returnTarget("portal", false, "near"), "near");
    EXPECT_EQ(returnTarget("", true, "near"), "near");
    EXPECT_EQ(returnTarget("", false, ""), "");
}

TEST(Scrolls, CanBind) {
    EXPECT_TRUE(canBind(true, false, 5));
    EXPECT_TRUE(canBind(true, true, 1));
    EXPECT_TRUE(canBind(false, false, 1));
    EXPECT_FALSE(canBind(false, false, 2));
    EXPECT_FALSE(canBind(false, true, 1));
}

TEST(Scrolls, BoundLore) {
    EXPECT_EQ(boundLore("Bound to: {}", "Home"), "§r§7Bound to: §bHome§7");
}
