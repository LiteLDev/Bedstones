#include <gtest/gtest.h>

#include "core/warpplate/WarpPlateLogic.h"

using namespace bs::core::warpplate;

TEST(WarpPlate, JavaHash) {
    EXPECT_EQ(javaHash(""), 0);
    EXPECT_EQ(javaHash("a"), 97);
    EXPECT_EQ(javaHash("hello"), 99162322);
    EXPECT_EQ(javaHash("polygenelubricants"), static_cast<std::int32_t>(0x80000000u)); // Integer.MIN_VALUE
    EXPECT_EQ(colorIndex("polygenelubricants"), 7);
}

TEST(WarpPlate, GalacticIdentifier) {
    EXPECT_EQ(galacticIdentifier("3f2a1b4c-9d8e-4f7a-b6c5-d4e3f2a1b0c9"), "fabcdefa");
    EXPECT_EQ(galacticIdentifier("12345678-1234-4123-8123-1234567890ab"), "ab");
    EXPECT_EQ(galacticIdentifier(""), "");
}

TEST(WarpPlate, ColorMapping) {
    EXPECT_EQ(chatFormattingId(7), 13);
    EXPECT_EQ(chatFormattingId(8), 5);
    EXPECT_EQ(chatFormattingId(0), 6);
    EXPECT_EQ(chatFormattingId(11), 11);
    EXPECT_EQ(formattingCode(13), 'd');
    EXPECT_EQ(formattingRgb(6), 0xFFAA00u);
    for (auto const* s : {"fabcdefa", "ab", "cafebabe", "deadbeef"}) {
        int i = colorIndex(s);
        EXPECT_GE(i, 0);
        EXPECT_LT(i, 15);
        int h = javaHash(s);
        EXPECT_EQ(i, (h < 0 ? -h : h) % 15);
    }
}

TEST(WarpPlate, UseTime) {
    EXPECT_EQ(useTime(15, 0, 0), 15);
    EXPECT_EQ(useTime(15, 1, 0), 14);  // 15 * 0.984 = 14.76
    EXPECT_EQ(useTime(15, 0, 1), 15);  // 15 * 1.016 = 15.24
    EXPECT_EQ(useTime(15, 0, 64), 30); // 15 * 2.024 clamped to 30
    EXPECT_EQ(useTime(15, 64, 0), 1);  // negative clamped to 1
    EXPECT_EQ(useTime(100, 10, 0), 84);
    EXPECT_EQ(useTime(100, 0, 10), 116);
    EXPECT_EQ(useTime(0, 0, 0), 1);
}

TEST(WarpPlate, Status) {
    EXPECT_EQ(idleStatus(ShardKind::None, false), Status::Empty);
    EXPECT_EQ(idleStatus(ShardKind::Dormant, true), Status::Attuning);
    EXPECT_EQ(idleStatus(ShardKind::Deepslate, true), Status::Locked);
    EXPECT_EQ(idleStatus(ShardKind::Attuned, false), Status::Idle);
    EXPECT_EQ(idleStatus(ShardKind::Crumbling, true), Status::RedstoneDisabled);
    EXPECT_TRUE(hasPotentialTarget(ShardKind::Attuned));
    EXPECT_FALSE(hasPotentialTarget(ShardKind::Deepslate));
    EXPECT_FALSE(hasPotentialTarget(ShardKind::None));
    EXPECT_TRUE(isWarpShard("bedstones:crumbling_attuned_shard"));
    EXPECT_FALSE(isWarpShard("minecraft:quartz"));
}

TEST(WarpPlate, ChooseTarget) {
    std::vector<Candidate> c{
        {0, false, "a"},
        {2, true,  "b"},
        {3, false, "c"}
    };
    int  last  = 0;
    auto first = [](int) { return 0; };
    // Round robin walks the list.
    EXPECT_EQ(chooseTarget(c, true, false, last, first)->target, "b");
    EXPECT_EQ(chooseTarget(c, true, false, last, first)->target, "c");
    EXPECT_EQ(chooseTarget(c, true, false, last, first)->target, "a");
    // Spider eye keeps single-use shards only.
    EXPECT_EQ(chooseTarget(c, false, true, last, [](int n) { return n - 1; })->target, "b");
    // Random uses the generator.
    EXPECT_EQ(chooseTarget(c, false, false, last, [](int) { return 2; })->target, "c");
    EXPECT_FALSE(chooseTarget({}, false, false, last, first).has_value());
    // Spider eye without crumbling shards keeps all.
    std::vector<Candidate> d{
        {0, false, "x"}
    };
    EXPECT_EQ(chooseTarget(d, false, true, last, first)->target, "x");
}
