#include "core/worldgen/Worldgen.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <limits>

namespace bs::core::worldgen {

namespace {

std::string lower(std::string_view s) {
    std::string out(s);
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

bool contains(std::string_view text, std::string_view part) { return text.find(part) != std::string_view::npos; }

struct StyleName {
    WildStyle        style;
    std::string_view config; ///< lower-case config value
    std::string_view path;   ///< block path
};
constexpr std::array<StyleName, 11> kStyles{{
    {WildStyle::Default, "default", "waystone"},
    {WildStyle::Mossy, "mossy", "mossy_waystone"},
    {WildStyle::Sandy, "sandy", "sandy_waystone"},
    {WildStyle::Blackstone, "blackstone", "blackstone_waystone"},
    {WildStyle::RedNetherBricks, "red_nether_bricks", "red_nether_bricks_waystone"},
    {WildStyle::Deepslate, "deepslate", "deepslate_waystone"},
    {WildStyle::EndStone, "end_stone", "end_stone_waystone"},
    {WildStyle::Purpur, "purpur", "purpur_waystone"},
    {WildStyle::Prismarine, "prismarine", "prismarine_waystone"},
    {WildStyle::MudBricks, "mud_bricks", "mud_bricks_waystone"},
    {WildStyle::Biome, "biome", "waystone"},
}};

std::uint64_t hashCell(std::uint64_t seed, int x, int z, std::uint64_t salt) {
    auto packed = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32) | static_cast<std::uint32_t>(z);
    return mix(seed ^ mix(packed ^ mix(salt)));
}

} // namespace

std::uint64_t mix(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

std::optional<WildStyle> wildStyleFromString(std::string_view s) {
    auto l = lower(s);
    for (auto const& e : kStyles) {
        if (e.config == l) return e.style;
    }
    return std::nullopt;
}

std::string_view stylePath(WildStyle style) {
    for (auto const& e : kStyles) {
        if (e.style == style) return e.path;
    }
    return "waystone";
}

std::optional<VillageMode> villageModeFromString(std::string_view s) {
    auto l = lower(s);
    if (l == "disabled") return VillageMode::Disabled;
    if (l == "regular") return VillageMode::Regular;
    if (l == "frequent") return VillageMode::Frequent;
    return std::nullopt;
}

std::string_view styleForBiome(std::string_view biomeName, int dim) {
    auto b = lower(biomeName);
    if (auto colon = b.find(':'); colon != std::string::npos) b = b.substr(colon + 1);
    // Nether biomes (Bedrock calls the nether wastes "hell").
    if (dim == 1 || b == "hell" || b == "nether_wastes" || b == "crimson_forest" || b == "warped_forest"
        || b == "soulsand_valley" || b == "soul_sand_valley" || b == "basalt_deltas") {
        return "blackstone_waystone";
    }
    if (dim == 2 || contains(b, "the_end") || contains(b, "end_")) return "end_stone_waystone";
    if (b == "deep_dark") return "deepslate_waystone";
    if (contains(b, "desert")) return "sandy_waystone";
    if (contains(b, "swamp") || contains(b, "mushroom") || contains(b, "jungle")) return "mossy_waystone";
    return "waystone";
}

std::string_view resolveStyle(WildStyle style, std::string_view biome, int dim) {
    return style == WildStyle::Biome ? styleForBiome(biome, dim) : stylePath(style);
}

std::string_view villageStyle(std::string_view biome) {
    return contains(lower(biome), "desert") ? "sandy_waystone" : "waystone";
}

int floorDiv(int a, int b) {
    int q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

Cell cellOf(int chunkX, int chunkZ, int n) {
    n = std::max(n, 1);
    return {floorDiv(chunkX, n), floorDiv(chunkZ, n)};
}

int maxOffset(int n) {
    n = std::max(n, 1);
    return std::min((n + 1) / 2, n - 1);
}

WildSpot wildSpot(std::uint64_t seed, Cell cell, int n) {
    n            = std::max(n, 1);
    auto h       = hashCell(seed, cell.x, cell.z, 0x5741595354ULL); // "WAYST"
    int  range   = maxOffset(n) + 1;
    WildSpot s;
    s.cell   = cell;
    s.chunkX = cell.x * n + static_cast<int>(h % static_cast<std::uint64_t>(range));
    s.chunkZ = cell.z * n + static_cast<int>((h >> 16) % static_cast<std::uint64_t>(range));
    s.blockX = s.chunkX * 16 + static_cast<int>((h >> 32) & 15);
    s.blockZ = s.chunkZ * 16 + static_cast<int>((h >> 36) & 15);
    s.facing = static_cast<int>((h >> 40) & 3);
    return s;
}

std::optional<WildSpot> spotInChunk(std::uint64_t seed, int chunkX, int chunkZ, int n) {
    auto s = wildSpot(seed, cellOf(chunkX, chunkZ, n), n);
    if (s.chunkX != chunkX || s.chunkZ != chunkZ) return std::nullopt;
    return s;
}

WildSpot nearestSpot(std::uint64_t seed, int blockX, int blockZ, int n) {
    n         = std::max(n, 1);
    auto home = cellOf(floorDiv(blockX, 16), floorDiv(blockZ, 16), n);
    WildSpot best{};
    double   bestDist = std::numeric_limits<double>::max();
    // The home cell's spot is less than sqrt(2) cells away; spots 3 cells away are at least 2
    // cells away, so a 7x7 search always holds the nearest one.
    for (int dx = -3; dx <= 3; ++dx) {
        for (int dz = -3; dz <= 3; ++dz) {
            auto   s = wildSpot(seed, {home.x + dx, home.z + dz}, n);
            double d = std::hypot(static_cast<double>(s.blockX - blockX), static_cast<double>(s.blockZ - blockZ));
            if (d < bestDist) bestDist = d, best = s;
        }
    }
    return best;
}

std::string cellKey(int dim, int n, Cell cell) {
    return std::to_string(dim) + ":" + std::to_string(n) + ":" + std::to_string(cell.x) + ":" + std::to_string(cell.z);
}

std::string dimensionName(int dim) {
    switch (dim) {
    case 0:
        return "minecraft:overworld";
    case 1:
        return "minecraft:the_nether";
    case 2:
        return "minecraft:the_end";
    default:
        return "dimension_" + std::to_string(dim);
    }
}

std::string normalizeDimension(std::string_view name) {
    auto l = lower(name);
    if (l == "nether" || l == "minecraft:nether") return "minecraft:the_nether";
    if (l == "end" || l == "minecraft:end") return "minecraft:the_end";
    if (l.starts_with("dimension_")) return l;
    if (l.find(':') == std::string::npos) l = "minecraft:" + l;
    return l;
}

bool dimensionAllowed(int dim, std::vector<std::string> const& allow, std::vector<std::string> const& deny) {
    auto name = dimensionName(dim);
    auto in   = [&name](std::vector<std::string> const& list) {
        return std::any_of(list.begin(), list.end(), [&name](std::string const& e) { return normalizeDimension(e) == name; });
    };
    if (!allow.empty() && !in(allow)) return false;
    return !in(deny);
}

bool villageGetsWaystone(VillageMode mode, std::uint64_t seed, int dim, int x, int z) {
    switch (mode) {
    case VillageMode::Disabled:
        return false;
    case VillageMode::Frequent:
        return true;
    case VillageMode::Regular:
        break;
    }
    auto h = hashCell(seed, x, z, 0x56494C4CULL + static_cast<std::uint64_t>(dim)); // "VILL"
    // Top 53 bits as a uniform double in [0, 1).
    double roll = static_cast<double>(h >> 11) * (1.0 / 9007199254740992.0);
    return roll < kRegularVillageChance;
}

std::string villageKey(int dim, int x, int z) {
    return std::to_string(dim) + ":" + std::to_string(x) + ":" + std::to_string(z);
}

std::optional<VillageKey> parseVillageKey(std::string_view key) {
    int  values[3]{};
    auto p   = key.data();
    auto end = key.data() + key.size();
    for (int i = 0; i < 3; ++i) {
        auto [next, ec] = std::from_chars(p, end, values[i]);
        if (ec != std::errc{}) return std::nullopt;
        p = next;
        if (i < 2) {
            if (p == end || *p != ':') return std::nullopt;
            ++p;
        }
    }
    if (p != end) return std::nullopt;
    return VillageKey{values[0], values[1], values[2]};
}

} // namespace bs::core::worldgen
