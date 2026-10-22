#pragma once

// Block helpers (server thread): typed custom states, two-tall placement, positions.

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/Waystone.h"

class Block;
class BlockPos;
class BlockSource;
class Player;
class Actor;

namespace bs::game {

/// The block's full id ("minecraft:stone", "bedstones:waystone", ...).
[[nodiscard]] std::string blockId(Block const& block);
/// The block at `pos` ("" when the chunk isn't loaded).
[[nodiscard]] std::string blockIdAt(BlockSource& region, BlockPos const& pos);

/// Integer / boolean custom state of `block` (nullopt if the block has no such state).
[[nodiscard]] std::optional<int> intState(Block const& block, std::string_view state);

/// The default state of `id` with `states` applied (unknown states are ignored). nullptr if the
/// block id is not registered (behavior pack missing).
[[nodiscard]] Block const* blockWith(std::string_view id, std::vector<std::pair<std::string_view, int>> const& states = {});

/// Sets a block (neighbour + client updates). Returns false on failure.
bool setBlock(BlockSource& region, BlockPos const& pos, Block const& block);
/// Replaces the block with air.
bool clearBlock(BlockSource& region, BlockPos const& pos);

/// The block at `pos` can be replaced by a placement (air, grass, water, ...).
[[nodiscard]] bool replaceable(BlockSource& region, BlockPos const& pos);

/// Places a two-tall block (`bedstones:upper` false/true) with `bedstones:facing` = `facing` at
/// `pos` and above. Fails (returns false, places nothing) when either cell is not replaceable or
/// the top is above the build limit.
bool placeTwoTall(BlockSource& region, BlockPos const& pos, std::string_view id, int facing);
/// For a two-tall block cell: the position of its lower half (pos itself, or pos - 1 for the upper).
[[nodiscard]] BlockPos lowerHalf(BlockSource& region, BlockPos const& pos);

/// Java horizontal facing (0 S, 1 W, 2 N, 3 E) the player looks along; a placed block faces the
/// opposite (towards the player) like Waystones does.
[[nodiscard]] int playerFacing(Actor const& actor);
[[nodiscard]] inline int opposite(int facing) { return (facing + 2) & 3; }
/// Unit offset (dx, dz) of a Java 2D facing.
[[nodiscard]] std::pair<int, int> facingOffset(int facing);

[[nodiscard]] core::GlobalPos globalPos(BlockSource const& region, BlockPos const& pos);
[[nodiscard]] core::GlobalPos globalPos(int dim, BlockPos const& pos);
[[nodiscard]] BlockPos        blockPos(core::GlobalPos const& pos);

} // namespace bs::game
