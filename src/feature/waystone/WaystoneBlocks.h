#pragma once

// U1: the ten two-tall waystone blocks (docs/SPEC.md §1.1, §1.2, §3.2, §3.4).
//
// Server side only (also runs in the client build for the integrated server):
// - placing a waystone item places both halves facing the player, creates (or, for a silk-touched
//   item, restores) the waystone record, activates it for the placer and opens the Edit screen;
// - breaking either half removes both; survival + pickaxe drops one item (silk touch keeps the
//   record as `transient` and puts its uid on the item), otherwise the record is deleted;
// - right-click activates / opens the selection menu, shift + right-click (empty hands) opens Edit.
//
// World edits triggered by interact / destroy events run on the next server tick (a cancelled
// interact event rolls back block changes made during it).

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Waystone.h"

class Block;
class BlockPos;
class BlockSource;
class ItemStack;
class Player;

namespace bs::feature::waystone {

/// "bedstones:<style>" of one of the ten waystone styles.
[[nodiscard]] bool isWaystoneId(std::string_view fullId);
[[nodiscard]] bool isWaystoneBlock(Block const& block);

enum class PlaceResult { Ok, NotWaystone, NoRoom, Failed };
struct Placed {
    PlaceResult result = PlaceResult::Failed;
    std::string uid;
    bool        restored = false; ///< a silk-touched item brought its record back
};

/// Places the waystone `item` with its lower half at `pos` (does not consume the item), creates or
/// restores the record and activates it for `player`. `facing` defaults to facing the player.
/// `openEdit` opens the Edit screen afterwards (Waystones does this for player placements).
Placed place(
    Player&            player,
    BlockSource&       region,
    BlockPos const&    pos,
    ItemStack const&   item,
    std::optional<int> facing   = std::nullopt,
    bool               openEdit = true
);

/// The record of the waystone block at `pos` (either half). With `create`, a waystone block without
/// a record (e.g. set by a command) gets a fresh one (origin unknown, no owner).
core::WaystoneRecord* recordAt(BlockSource& region, BlockPos const& pos, bool create);

/// Right-click on the waystone at `pos` (either half) after the item checks: shift opens Edit,
/// otherwise activate or open the selection menu. `heldId` is the main-hand item id (creative
/// debug tools).
void use(Player& player, BlockSource& region, BlockPos const& pos, bool sneaking, std::string const& heldId);

/// Activates the record for `player` with Waystones' feedback (naming of generated waystones,
/// chat message, sound, particles, observers). Returns false if it was already activated.
bool activate(Player& player, BlockSource& region, core::WaystoneRecord& record);

/// Remembers that `player` is breaking the waystone at `pos` (tool, silk touch, creative); the
/// removal handler (next tick) uses it to decide the drop.
void noteBreak(Player& player, BlockSource& region, BlockPos const& pos);

/// The item dropped for `record` when broken with silk touch (uid + aqua name lore).
[[nodiscard]] ItemStack silkItem(core::WaystoneRecord const& record);

/// Processes queued removals and actions now (selftests; normally done every tick).
void flush();

/// Items dropped by the most recent removal (selftests).
[[nodiscard]] std::vector<ItemStack> const& lastDrops();

} // namespace bs::feature::waystone
