#pragma once

// Block behaviour shared by sharestones and portstones (two-tall dyed blocks, docs/SPEC.md §1.1,
// §1.3, §1.4): placement completes the upper half facing the player, right-click dispatches to the
// family (sharestone: selection / edit, portstone: selection), and either half going away removes
// the other one (plus the sharestone record) and drops the item for survival pickaxe breaks.
//
// World edits never happen inside the triggering event: placement, use and removal are queued and
// run on the next server tick (a cancelled interact event rolls back block changes made during it).

#include <optional>
#include <string>
#include <string_view>

#include "core/sharestone/Stones.h"

class BlockPos;
class BlockSource;
class Player;
class Block;
class ItemStack;

namespace bs::feature::sharestone {

using core::sharestone::Family;
using core::sharestone::StoneKind;

/// A sharestone / portstone found in the world.
struct StoneAt {
    StoneKind   kind;
    std::string id; ///< "bedstones:red_sharestone"
    int         dim = 0;
    int         x = 0, y = 0, z = 0; ///< lower half
    int         facing = 0;
};

/// Who placed a stone (kept from the placement event: the player may be gone a tick later).
struct Placer {
    std::string uuid;
    std::string name;
};

/// The stone kind of a block id ("bedstones:red_sharestone"), nullopt for other blocks.
[[nodiscard]] std::optional<StoneKind> stoneKindOf(std::string_view blockId);
/// The stone at `pos` (either half), nullopt if there is none.
[[nodiscard]] std::optional<StoneAt> stoneAt(BlockSource& region, BlockPos const& pos);

/// Finishes a player's placement of a stone whose lower half is at `lower`: sets both halves
/// (facing the player) and runs the family's placement behaviour (sharestone record + edit screen).
/// The upper cell must be free. Returns false (nothing changed) when it is not.
bool completePlacement(Player& player, BlockSource& region, BlockPos const& lower, StoneKind const& kind);

/// Right-click behaviour of the stone at `pos` (either half), as if `player` clicked it.
/// Returns false if there is no stone.
bool useStone(Player& player, BlockSource& region, BlockPos const& pos, bool sneaking);

/// Whether breaking a stone with `tool` drops it (survival and a pickaxe; never in creative).
[[nodiscard]] bool breakDrops(Player const& player, ItemStack const& tool);

/// Items dropped by broken stones since the mod was enabled (selftests).
[[nodiscard]] int dropCount();

} // namespace bs::feature::sharestone
