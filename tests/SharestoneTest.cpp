#include <gtest/gtest.h>

#include "core/Waystone.h"
#include "core/sharestone/Stones.h"

using namespace bs::core::sharestone;

TEST(Sharestone, ParsesSharestones) {
    auto k = parse("red_sharestone");
    ASSERT_TRUE(k);
    EXPECT_EQ(k->family, Family::Sharestone);
    EXPECT_EQ(k->color, "red");
    EXPECT_TRUE(parse("light_blue_sharestone"));
    EXPECT_FALSE(parse("white_sharestone")); // there is no white sharestone
    EXPECT_FALSE(parse("teal_sharestone"));
    EXPECT_FALSE(parse("sharestone"));
    EXPECT_FALSE(parse("waystone"));
}

TEST(Sharestone, ParsesPortstones) {
    auto w = parse("white_portstone");
    ASSERT_TRUE(w);
    EXPECT_EQ(w->family, Family::Portstone);
    EXPECT_EQ(w->color, "white");
    EXPECT_TRUE(parse("light_gray_portstone"));
    EXPECT_FALSE(parse("portstone"));
    EXPECT_FALSE(parse("teal_portstone"));
}

TEST(Sharestone, AllColorsRoundTrip) {
    int sharestones = 0, portstones = 0;
    for (auto color : bs::core::kDyeColors) {
        for (auto family : {Family::Sharestone, Family::Portstone}) {
            StoneKind k{family, std::string(color)};
            auto      back = parse(path(k));
            if (back) {
                EXPECT_EQ(*back, k);
                (family == Family::Sharestone ? sharestones : portstones)++;
            }
        }
    }
    EXPECT_EQ(sharestones, 15);
    EXPECT_EQ(portstones, 16);
}

TEST(Sharestone, DisplayNames) {
    EXPECT_EQ(colorName("light_gray"), "Light Gray");
    EXPECT_EQ(displayName({Family::Sharestone, "red"}), "Red Sharestone");
    EXPECT_EQ(displayName({Family::Portstone, "white"}), "Portstone");
    EXPECT_EQ(displayName({Family::Portstone, "light_blue"}), "Light Blue Portstone");
}
