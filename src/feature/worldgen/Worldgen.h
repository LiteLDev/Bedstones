#pragma once

// U15 world generation: wild waystones on a seeded chunk grid and waystones in villages
// (docs/SPEC.md §3.11). Server thread only.
//
// Chunks are reported by two hooks: `Dimension::onChunkLoaded` (every load: wild waystones of
// cells whose chunk existed before the mod) and `LevelChunk::finalizePostProcessing` (freshly
// generated chunks: village detection via their meeting-point bell). Hook callbacks only queue
// the chunk; the work happens on the next server ticks. Cells / villages already handled are
// persisted in the world DB under "bs:worldgen".

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Waystone.h"
#include "core/worldgen/Worldgen.h"

class BlockPos;
class BlockSource;

namespace bs::feature::worldgen {

/// Config section `worldGen` (Waystones' world generation options).
struct WorldGenConfig {
    int         version                    = 1;
    std::string wildWaystoneStyle          = "BIOME"; ///< DEFAULT|MOSSY|SANDY|BLACKSTONE|RED_NETHER_BRICKS|DEEPSLATE|END_STONE|PURPUR|PRISMARINE|MUD_BRICKS|BIOME
    int         chunksBetweenWildWaystones = 25;      ///< 0 disables wild waystones
    std::vector<std::string> wildWaystonesDimensionAllowList{"minecraft:overworld", "minecraft:the_nether", "minecraft:the_end"};
    std::vector<std::string> wildWaystonesDimensionDenyList{};
    std::string              spawnInVillages = "REGULAR"; ///< DISABLED|REGULAR|FREQUENT
};

enum class PlaceResult {
    Placed,
    NoSpace,      ///< no suitable ground / the cells are not free (the cell counts as done)
    Occupied,     ///< a waystone record already exists there (done)
    BlockMissing, ///< the style's block is not registered (wild cells: retried on later loads)
    NotLoaded,    ///< the chunk is not loaded (retried on the next load)
    Disabled,     ///< config / dimension filters
};
[[nodiscard]] std::string_view toString(PlaceResult r);

// ---- config helpers --------------------------------------------------------------------------
[[nodiscard]] core::worldgen::WildStyle   wildStyle();
[[nodiscard]] core::worldgen::VillageMode villageMode();
/// Wild waystones are enabled in `dim` (grid size > 0, allow / deny lists).
[[nodiscard]] bool wildEnabledIn(int dim);
/// The world seed (0 without a level).
[[nodiscard]] std::uint64_t worldSeed();

// ---- placement ---------------------------------------------------------------------------------
/// y of the lowest block of a wild waystone at column (x, z): above the ocean-floor surface, or
/// in dimensions with a ceiling the first air-above-solid spot scanning down. nullopt if none.
[[nodiscard]] std::optional<int> wildSurfaceY(BlockSource& region, int x, int z, bool ceiling);

/// Places a two-tall waystone of `style` ("waystone", "mossy_waystone", ...) at `pos` and creates
/// its record (no name, no owner, visibility Activation, `origin`). `blockOverride` replaces the
/// block id when the style's block is not registered (selftests without the blocks unit).
PlaceResult placeWaystone(
    BlockSource&      region,
    BlockPos const&   pos,
    std::string_view  style,
    int               facing,
    core::Origin      origin,
    std::string*      uidOut        = nullptr,
    std::string_view  blockOverride = {}
);

/// Generates the wild waystone of `spot` in `dim` if its chunk is loaded and it is not done yet.
PlaceResult generateWild(int dim, core::worldgen::WildSpot const& spot);

/// A meeting-point bell in chunk (chunkX, chunkZ), searched a few blocks below the surface.
[[nodiscard]] std::optional<BlockPos> findBell(BlockSource& region, int chunkX, int chunkZ);
/// Where a village waystone goes near the village centre: solid ground, open sky, 4-12 blocks
/// from `center`, within 3 blocks of its height.
[[nodiscard]] std::optional<BlockPos> villageSpot(BlockSource& region, BlockPos const& center);
/// Handles a village found at `center` (once per village; chance per `spawnInVillages`).
/// `NotLoaded` while its surroundings load; `BlockMissing` villages are not remembered.
PlaceResult generateVillage(int dim, BlockPos const& center, std::string* uidOut = nullptr);

/// Processes a chunk report now (what the hooks queue): wild cell check, and for freshly
/// generated chunks the village scan. Returns the wild result (`Disabled` when the chunk holds
/// no wild spot).
PlaceResult processChunk(int dim, int chunkX, int chunkZ, bool fresh);

// ---- persisted state ("bs:worldgen") ------------------------------------------------------------
[[nodiscard]] bool isCellDone(std::string const& key);
void               markCellDone(std::string const& key);
void               forgetCell(std::string const& key);
[[nodiscard]] bool isVillageDone(std::string const& key);
void               forgetVillage(std::string const& key);
/// Writes the state now (also done on autosave).
void saveState();
/// Drops the in-memory state; the next access reloads it from the world DB.
void reloadState();

/// Debug: block used instead of a missing style block ("" = off). Not persisted.
void                             setTestBlock(std::string id);
[[nodiscard]] std::string const& testBlock();

} // namespace bs::feature::worldgen
