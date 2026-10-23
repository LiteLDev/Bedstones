#include <gtest/gtest.h>

#include <random>

#include "core/naming/NameGenerator.h"

using namespace bs::core::naming;

namespace {

/// Deterministic generator returning a fixed sequence (modulo bound), then zeros.
Random sequence(std::vector<std::uint32_t> values) {
    auto state = std::make_shared<std::pair<std::vector<std::uint32_t>, std::size_t>>(std::move(values), 0);
    return [state](std::uint32_t bound) -> std::uint32_t {
        auto& [v, i] = *state;
        std::uint32_t x = i < v.size() ? v[i] : 0;
        ++i;
        return x % bound;
    };
}

Random seeded(unsigned seed) {
    auto eng = std::make_shared<std::mt19937>(seed);
    return [eng](std::uint32_t bound) { return std::uniform_int_distribution<std::uint32_t>(0, bound - 1)(*eng); };
}

bool startsWithAny(std::string_view s, auto const& table, std::size_t& len) {
    for (auto t : table) {
        if (s.starts_with(t)) {
            len = t.size();
            return true;
        }
    }
    return false;
}

} // namespace

TEST(Naming, MrPorkUsesTheThreeTables) {
    // Kr + mi + ark (the example from docs/SPEC.md).
    EXPECT_EQ(mrPork(sequence({0, 2, 2})), "Krmiark");
    EXPECT_EQ(mrPork(sequence({24, 19, 12})), "Lukurakmur");
    auto rng = seeded(7);
    for (int i = 0; i < 200; ++i) {
        auto        n = mrPork(rng);
        std::size_t len = 0;
        ASSERT_TRUE(startsWithAny(n, kMrPork1, len)) << n;
        EXPECT_GE(n.size(), len + 2) << n;
    }
}

TEST(Naming, Roman) {
    EXPECT_EQ(toRoman(1), "I");
    EXPECT_EQ(toRoman(2), "II");
    EXPECT_EQ(toRoman(3), "III");
    EXPECT_EQ(toRoman(4), "IV");
    EXPECT_EQ(toRoman(9), "IX");
    EXPECT_EQ(toRoman(14), "XIV");
    EXPECT_EQ(toRoman(40), "XL");
    EXPECT_EQ(toRoman(1994), "MCMXCIV");
    EXPECT_EQ(toRoman(0), "");
}

TEST(Naming, Modes) {
    EXPECT_EQ(modeFromString("PRESET_FIRST"), Mode::PresetFirst);
    EXPECT_EQ(modeFromString("random_only"), Mode::RandomOnly);
    EXPECT_EQ(modeFromString("Preset_Only"), Mode::PresetOnly);
    EXPECT_EQ(modeFromString("MIXED"), Mode::Mixed);
    EXPECT_FALSE(modeFromString("bogus").has_value());
    EXPECT_EQ(toString(Mode::Mixed), "MIXED");
}

TEST(Naming, TemplateExpansion) {
    EXPECT_EQ(expandTemplate("{MrPork}", "Krmiark", "Plains"), "Krmiark");
    EXPECT_EQ(expandTemplate("{Biome} {MrPork}", "Krmiark", "Cherry Grove"), "Cherry Grove Krmiark");
    EXPECT_EQ(expandTemplate("{MrPork} of {Biome}", "Zed", ""), "Zed of");
    EXPECT_EQ(expandTemplate("{MrPork}-{MrPork}", "A", ""), "A-A");
    EXPECT_EQ(biomeDisplayName("minecraft:cherry_grove"), "Cherry Grove");
    EXPECT_EQ(biomeDisplayName("plains"), "Plains");
    EXPECT_EQ(biomeDisplayName("Deep Dark"), "Deep Dark");
    EXPECT_EQ(biomeDisplayName(""), "");
}

TEST(Naming, DuplicatesGetRomanNumerals) {
    NameGenerator gen;
    Settings      s{Mode::PresetOnly, "{MrPork}", {"Spawn"}};
    auto          rng = seeded(1);
    EXPECT_EQ(gen.generate(s, "", rng), "Spawn");
    EXPECT_EQ(gen.generate(s, "", rng), "Spawn II");
    EXPECT_EQ(gen.generate(s, "", rng), "Spawn III");
    EXPECT_EQ(gen.generate(s, "", rng), "Spawn IV");
    EXPECT_EQ(gen.usedNames().size(), 4u);
    EXPECT_EQ(gen.resolveDuplicate("Other"), "Other");
}

TEST(Naming, PresetFirstUsesUnusedPresetsThenTemplate) {
    NameGenerator gen;
    Settings      s{Mode::PresetFirst, "{Biome} {MrPork}", {"A", "B"}};
    auto          rng = seeded(3);
    std::set<std::string> got{gen.generate(s, "minecraft:plains", rng), gen.generate(s, "minecraft:plains", rng)};
    EXPECT_EQ(got, (std::set<std::string>{"A", "B"}));
    auto third = gen.generate(s, "minecraft:plains", rng);
    EXPECT_TRUE(third.starts_with("Plains ")) << third;
}

TEST(Naming, RandomOnlyIgnoresPresets) {
    NameGenerator gen;
    Settings      s{Mode::RandomOnly, "{MrPork}", {"Preset"}};
    auto          rng = seeded(5);
    for (int i = 0; i < 20; ++i) EXPECT_NE(gen.generate(s, "", rng), "Preset");
}

TEST(Naming, PresetOnlyWithoutPresetsFallsBackToTemplate) {
    NameGenerator gen;
    Settings      s{Mode::PresetOnly, "{MrPork}", {}};
    EXPECT_EQ(gen.generate(s, "", sequence({0, 2, 2})), "Krmiark");
}

TEST(Naming, MixedPicksBoth) {
    NameGenerator gen;
    Settings      s{Mode::Mixed, "{MrPork}", {"P1", "P2", "P3", "P4", "P5", "P6", "P7", "P8"}};
    auto          rng      = seeded(11);
    int           presets  = 0;
    int           randoms  = 0;
    for (int i = 0; i < 8; ++i) {
        auto n = gen.generate(s, "", rng);
        (n.size() == 2 && n[0] == 'P' ? presets : randoms)++;
    }
    EXPECT_GT(presets, 0);
    EXPECT_GT(randoms, 0);
}

TEST(Naming, AllGeneratedNamesAreUnique) {
    NameGenerator gen;
    Settings      s{Mode::RandomOnly, "{MrPork}", {}};
    auto          rng = seeded(42);
    std::set<std::string> seen;
    // 25*20*13 = 6500 base names; 8000 draws force many numeral suffixes.
    for (int i = 0; i < 8000; ++i) EXPECT_TRUE(seen.insert(gen.generate(s, "", rng)).second);
}

TEST(Naming, UsedNamesRoundTrip) {
    NameGenerator gen;
    gen.markUsed("Alpha");
    gen.markUsed("Beta");
    NameGenerator back;
    back.loadJson(nlohmann::json::parse(gen.toJson().dump()));
    EXPECT_EQ(back.usedNames(), gen.usedNames());
    EXPECT_EQ(back.resolveDuplicate("Alpha"), "Alpha II");
    back.loadJson(nlohmann::json::array());
    EXPECT_TRUE(back.usedNames().empty());
}
