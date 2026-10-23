#include <gtest/gtest.h>

#include "core/inventorybutton/InventoryButton.h"

using namespace bs::core;
using namespace bs::core::inventorybutton;

TEST(InventoryButton, ParseMode) {
    EXPECT_EQ(parseMode(""), Mode::None);
    EXPECT_EQ(parseMode("NONE"), Mode::None);
    EXPECT_EQ(parseMode("none"), Mode::None);
    EXPECT_EQ(parseMode("  "), Mode::None);
    EXPECT_EQ(parseMode("NEAREST"), Mode::Nearest);
    EXPECT_EQ(parseMode(" nearest "), Mode::Nearest);
    EXPECT_EQ(parseMode("ANY"), Mode::Any);
    EXPECT_EQ(parseMode("Spawn"), Mode::Named);
    for (auto m : {Mode::None, Mode::Nearest, Mode::Any, Mode::Named}) EXPECT_EQ(modeFromString(toString(m)), m);
}

TEST(InventoryButton, NearestSameDimension) {
    std::vector<Candidate> c{
        {"a", "A", "", {0, 100, 64, 0}},
        {"b", "B", "", {0, 10, 64, 0} },
        {"c", "C", "", {1, 0, 64, 0}  }, // other dimension, closest in coordinates
    };
    EXPECT_EQ(nearest(c, 0, 0, 64, 0), std::optional<std::size_t>(1));
    EXPECT_EQ(nearest(c, 1, 50, 64, 0), std::optional<std::size_t>(2));
    EXPECT_FALSE(nearest(c, 2, 0, 64, 0).has_value());
    EXPECT_FALSE(nearest({}, 0, 0, 0, 0).has_value());
    // Ties keep activation order.
    std::vector<Candidate> tie{
        {"x", "X", "", {0, 5, 0, 0} },
        {"y", "Y", "", {0, -6, 0, 0}}
    };
    EXPECT_EQ(nearest(tie, 0, 0, 0.5, 0.5), std::optional<std::size_t>(0));
}

TEST(InventoryButton, ByName) {
    std::vector<Candidate> c{
        {"a", "Home", "",     {}},
        {"b", "Farm", "Home", {}},
        {"c", "Mine", "",     {}}
    };
    EXPECT_EQ(byName(c, "Home"), std::optional<std::size_t>(0)); // the real name wins over an alias
    EXPECT_EQ(byName(c, "Mine"), std::optional<std::size_t>(2));
    EXPECT_FALSE(byName(c, "home").has_value());
    EXPECT_FALSE(byName(c, "").has_value());
    std::vector<Candidate> alias{
        {"b", "Farm", "Base", {}}
    };
    EXPECT_EQ(byName(alias, "Base"), std::optional<std::size_t>(0));
}

TEST(InventoryButton, CooldownSeconds) {
    EXPECT_EQ(cooldownSeconds(0), 0);
    EXPECT_EQ(cooldownSeconds(-3), 0);
    EXPECT_EQ(cooldownSeconds(0.2), 1);
    EXPECT_EQ(cooldownSeconds(299.0), 299);
    EXPECT_EQ(cooldownSeconds(299.4), 300);
}

TEST(InventoryButton, StateRoundTrip) {
    ButtonState s;
    s.mode                       = Mode::Named;
    s.hasTarget                  = true;
    s.targetId                   = "uid";
    s.targetName                 = "Spawn";
    s.canAfford                  = false;
    s.cooldownSeconds            = 42;
    s.requirement.activeCooldown = 41.5;
    s.x                          = 1;
    s.y                          = 2;
    s.creativeX                  = 3;
    s.creativeY                  = 4;
    auto b                       = ButtonState::fromJson(s.toJson());
    EXPECT_EQ(b.mode, Mode::Named);
    EXPECT_TRUE(b.hasTarget);
    EXPECT_EQ(b.targetName, "Spawn");
    EXPECT_FALSE(b.canAfford);
    EXPECT_EQ(b.cooldownSeconds, 42);
    EXPECT_DOUBLE_EQ(b.requirement.activeCooldown, 41.5);
    EXPECT_EQ(b.x + b.y + b.creativeX + b.creativeY, 10);
    auto d = ButtonState::fromJson(nlohmann::json::array());
    EXPECT_EQ(d.mode, Mode::None);
    EXPECT_EQ(d.x, 58);
}
