#include <gtest/gtest.h>

#include <cmath>
#include <set>

#include "core/worldgen/Worldgen.h"

using namespace bs::core::worldgen;

TEST(Worldgen, FloorDivAndCells) {
    EXPECT_EQ(floorDiv(7, 25), 0);
    EXPECT_EQ(floorDiv(-1, 25), -1);
    EXPECT_EQ(floorDiv(-25, 25), -1);
    EXPECT_EQ(floorDiv(-26, 25), -2);
    EXPECT_EQ(cellOf(24, -1, 25), (Cell{0, -1}));
    EXPECT_EQ(cellOf(25, 50, 25), (Cell{1, 2}));
    EXPECT_EQ(maxOffset(25), 13);
    EXPECT_EQ(maxOffset(2), 1);
    EXPECT_EQ(maxOffset(1), 0);
}

TEST(Worldgen, SpotIsDeterministicAndInsideItsCell) {
    for (int n : {1, 2, 5, 25, 32}) {
        for (int cx = -3; cx <= 3; ++cx) {
            for (int cz = -3; cz <= 3; ++cz) {
                auto a = wildSpot(12345, {cx, cz}, n);
                auto b = wildSpot(12345, {cx, cz}, n);
                EXPECT_EQ(a.chunkX, b.chunkX);
                EXPECT_EQ(a.blockZ, b.blockZ);
                EXPECT_EQ(a.facing, b.facing);
                EXPECT_EQ(cellOf(a.chunkX, a.chunkZ, n), (Cell{cx, cz}));
                EXPECT_LE(a.chunkX - cx * n, maxOffset(n));
                EXPECT_LE(a.chunkZ - cz * n, maxOffset(n));
                EXPECT_EQ(a.blockX >> 4, a.chunkX);
                EXPECT_EQ(a.blockZ >> 4, a.chunkZ);
                EXPECT_GE(a.facing, 0);
                EXPECT_LE(a.facing, 3);
            }
        }
    }
}

TEST(Worldgen, ExactlyOneChunkPerCell) {
    int n = 5;
    for (int gx = -2; gx <= 2; ++gx) {
        for (int gz = -2; gz <= 2; ++gz) {
            int hits = 0;
            for (int x = gx * n; x < (gx + 1) * n; ++x) {
                for (int z = gz * n; z < (gz + 1) * n; ++z) {
                    if (spotInChunk(99, x, z, n)) ++hits;
                }
            }
            EXPECT_EQ(hits, 1) << "cell " << gx << "," << gz;
        }
    }
}

TEST(Worldgen, SeedChangesTheLayout) {
    int differ = 0;
    for (int i = 0; i < 20; ++i) {
        auto a = wildSpot(1, {i, -i}, 25);
        auto b = wildSpot(2, {i, -i}, 25);
        if (a.blockX != b.blockX || a.blockZ != b.blockZ) ++differ;
    }
    EXPECT_GT(differ, 15);
}

TEST(Worldgen, NearestSpot) {
    auto s = nearestSpot(7, 0, 0, 25);
    auto h = wildSpot(7, s.cell, 25);
    EXPECT_EQ(s.blockX, h.blockX);
    // Nothing in the 5x5 cells around is closer.
    double best = std::hypot(double(s.blockX), double(s.blockZ));
    for (int x = -2; x <= 2; ++x) {
        for (int z = -2; z <= 2; ++z) {
            auto o = wildSpot(7, {x, z}, 25);
            EXPECT_GE(std::hypot(double(o.blockX), double(o.blockZ)) + 1e-9, best);
        }
    }
    EXPECT_EQ(cellKey(1, 25, {-2, 3}), "1:25:-2:3");
}

TEST(Worldgen, StyleByBiome) {
    EXPECT_EQ(styleForBiome("desert", 0), "sandy_waystone");
    EXPECT_EQ(styleForBiome("minecraft:desert_hills", 0), "sandy_waystone");
    EXPECT_EQ(styleForBiome("swampland", 0), "mossy_waystone");
    EXPECT_EQ(styleForBiome("mangrove_swamp", 0), "mossy_waystone");
    EXPECT_EQ(styleForBiome("mushroom_island", 0), "mossy_waystone");
    EXPECT_EQ(styleForBiome("bamboo_jungle", 0), "mossy_waystone");
    EXPECT_EQ(styleForBiome("jungle_edge", 0), "mossy_waystone");
    EXPECT_EQ(styleForBiome("hell", 1), "blackstone_waystone");
    EXPECT_EQ(styleForBiome("crimson_forest", 1), "blackstone_waystone");
    EXPECT_EQ(styleForBiome("the_end", 2), "end_stone_waystone");
    EXPECT_EQ(styleForBiome("deep_dark", 0), "deepslate_waystone");
    EXPECT_EQ(styleForBiome("plains", 0), "waystone");
    EXPECT_EQ(styleForBiome("deep_ocean", 0), "waystone");
    EXPECT_EQ(resolveStyle(WildStyle::Purpur, "desert", 0), "purpur_waystone");
    EXPECT_EQ(resolveStyle(WildStyle::Biome, "desert", 0), "sandy_waystone");
    EXPECT_EQ(villageStyle("desert"), "sandy_waystone");
    EXPECT_EQ(villageStyle("plains"), "waystone");
}

TEST(Worldgen, ConfigValues) {
    EXPECT_EQ(wildStyleFromString("BIOME"), WildStyle::Biome);
    EXPECT_EQ(wildStyleFromString("red_nether_bricks"), WildStyle::RedNetherBricks);
    EXPECT_EQ(stylePath(WildStyle::MudBricks), "mud_bricks_waystone");
    EXPECT_FALSE(wildStyleFromString("granite").has_value());
    EXPECT_EQ(villageModeFromString("FREQUENT"), VillageMode::Frequent);
    EXPECT_FALSE(villageModeFromString("sometimes").has_value());
}

TEST(Worldgen, DimensionFilters) {
    std::vector<std::string> allow{"overworld", "minecraft:the_nether", "the_end"};
    EXPECT_TRUE(dimensionAllowed(0, allow, {}));
    EXPECT_TRUE(dimensionAllowed(1, allow, {}));
    EXPECT_TRUE(dimensionAllowed(2, allow, {}));
    EXPECT_FALSE(dimensionAllowed(0, allow, {"minecraft:overworld"}));
    EXPECT_FALSE(dimensionAllowed(1, {"overworld"}, {}));
    EXPECT_TRUE(dimensionAllowed(1, {}, {"the_end"}));
    EXPECT_FALSE(dimensionAllowed(2, {}, {"END"}));
    EXPECT_EQ(normalizeDimension("Nether"), "minecraft:the_nether");
}

TEST(Worldgen, VillageChance) {
    EXPECT_FALSE(villageGetsWaystone(VillageMode::Disabled, 1, 0, 0, 0));
    EXPECT_TRUE(villageGetsWaystone(VillageMode::Frequent, 1, 0, 0, 0));
    int yes = 0;
    for (int i = 0; i < 4000; ++i) {
        if (villageGetsWaystone(VillageMode::Regular, 42, 0, i * 97, -i * 31)) ++yes;
    }
    EXPECT_NEAR(yes / 4000.0, kRegularVillageChance, 0.03);
    EXPECT_EQ(villageGetsWaystone(VillageMode::Regular, 42, 0, 5, 6), villageGetsWaystone(VillageMode::Regular, 42, 0, 5, 6));
}

TEST(Worldgen, VillageKeys) {
    auto k = parseVillageKey(villageKey(1, -20, 300));
    ASSERT_TRUE(k.has_value());
    EXPECT_EQ(k->dim, 1);
    EXPECT_EQ(k->x, -20);
    EXPECT_EQ(k->z, 300);
    EXPECT_FALSE(parseVillageKey("1:2").has_value());
    EXPECT_FALSE(parseVillageKey("1:2:3:4").has_value());
}
