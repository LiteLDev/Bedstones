#pragma once

// World generation maths (pure C++): the wild waystone grid, style by biome, dimension filters
// and the village chance (docs/SPEC.md §3.11). Everything is deterministic from the world seed.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bs::core::worldgen {

// ---- config values --------------------------------------------------------------------------

/// `wildWaystoneStyle` (Waystones' `WorldGenStyle`).
enum class WildStyle {
    Default,
    Mossy,
    Sandy,
    Blackstone,
    RedNetherBricks,
    Deepslate,
    EndStone,
    Purpur,
    Prismarine,
    MudBricks,
    Biome,
};
/// "BIOME", "mossy", "red_nether_bricks", ... (case-insensitive). nullopt if unknown.
[[nodiscard]] std::optional<WildStyle> wildStyleFromString(std::string_view s);
/// The block path of a fixed style ("waystone", "mossy_waystone", ...). `Biome` -> "waystone".
[[nodiscard]] std::string_view stylePath(WildStyle style);

/// `spawnInVillages`.
enum class VillageMode { Disabled, Regular, Frequent };
[[nodiscard]] std::optional<VillageMode> villageModeFromString(std::string_view s);

// ---- style by biome ---------------------------------------------------------------------------

/// The waystone style path for a biome (Bedrock biome names, e.g. "desert", "swampland",
/// "mangrove_swamp", "mushroom_island", "bamboo_jungle", "crimson_forest", "the_end",
/// "deep_dark") in dimension `dim` (0 overworld, 1 nether, 2 end). Mirrors Waystones'
/// `has_structure/<style>` biome tags.
[[nodiscard]] std::string_view styleForBiome(std::string_view biome, int dim);
/// The style to place: the configured fixed style, or `styleForBiome` for `Biome`.
[[nodiscard]] std::string_view resolveStyle(WildStyle style, std::string_view biome, int dim);
/// Village waystones: sandy in desert villages, the default waystone elsewhere.
[[nodiscard]] std::string_view villageStyle(std::string_view biome);

// ---- wild waystone grid -------------------------------------------------------------------------

struct Cell {
    int x = 0, z = 0;
    friend bool operator==(Cell const&, Cell const&) = default;
};

/// Floor division (rounds towards negative infinity).
[[nodiscard]] int floorDiv(int a, int b);
/// The grid cell (of `n` x `n` chunks) holding chunk (chunkX, chunkZ). `n` >= 1.
[[nodiscard]] Cell cellOf(int chunkX, int chunkZ, int n);
/// Largest chunk offset of the waystone inside a cell: ceil(n / 2), kept inside the cell.
[[nodiscard]] int maxOffset(int n);

/// Where a cell's wild waystone goes.
struct WildSpot {
    Cell cell;
    int  chunkX = 0, chunkZ = 0; ///< the chunk holding the waystone
    int  blockX = 0, blockZ = 0; ///< absolute block column
    int  facing = 0;             ///< Java 2D facing 0..3
};

/// The (single) wild waystone spot of `cell`.
[[nodiscard]] WildSpot wildSpot(std::uint64_t seed, Cell cell, int n);
/// The spot if chunk (chunkX, chunkZ) is the one holding its cell's waystone.
[[nodiscard]] std::optional<WildSpot> spotInChunk(std::uint64_t seed, int chunkX, int chunkZ, int n);
/// The spot nearest to block column (blockX, blockZ) (searches the surrounding cells).
[[nodiscard]] WildSpot nearestSpot(std::uint64_t seed, int blockX, int blockZ, int n);
/// Persistence key of a cell: "<dim>:<n>:<x>:<z>".
[[nodiscard]] std::string cellKey(int dim, int n, Cell cell);

// ---- dimensions -----------------------------------------------------------------------------------

/// "minecraft:overworld" / "minecraft:the_nether" / "minecraft:the_end" / "dimension_<id>".
[[nodiscard]] std::string dimensionName(int dim);
/// Lower-case, "minecraft:" prefix added when missing; "nether" / "end" aliases accepted.
[[nodiscard]] std::string normalizeDimension(std::string_view name);
/// Waystones' rule: in the allow list (when it is not empty) and not in the deny list.
[[nodiscard]] bool dimensionAllowed(int dim, std::vector<std::string> const& allow, std::vector<std::string> const& deny);

// ---- villages ---------------------------------------------------------------------------------------

/// REGULAR: chance that a village gets a waystone. Waystones adds its waystone piece with weight 1
/// to the village house pools; with a pool weight of about 65 and about 10 house pieces per
/// village that is 1 - (64/65)^10 = 14.4 %, rounded to 15 %.
inline constexpr double kRegularVillageChance = 0.15;
/// Whether the village whose centre is at (x, z) in `dim` gets a waystone (deterministic).
[[nodiscard]] bool villageGetsWaystone(VillageMode mode, std::uint64_t seed, int dim, int x, int z);
/// Persistence key of a village: "<dim>:<x>:<z>".
[[nodiscard]] std::string villageKey(int dim, int x, int z);
struct VillageKey {
    int dim = 0, x = 0, z = 0;
};
/// Parses `villageKey` output; nullopt if malformed.
[[nodiscard]] std::optional<VillageKey> parseVillageKey(std::string_view key);

/// splitmix64 finaliser (exposed for tests).
[[nodiscard]] std::uint64_t mix(std::uint64_t x);

} // namespace bs::core::worldgen
