#pragma once

// Sharestone behaviour (docs/SPEC.md §1.3, §3.9): a record per block, no activation, right-click
// lists every sharestone of the same colour, shift + right-click edits.

#include <string>

#include "feature/sharestone/StoneBlocks.h"

namespace bs::feature::sharestone {

/// A player finished placing a sharestone: creates its record (owned by `placer`) and opens the
/// Edit screen for `player` (null: the placer is no longer online).
void onPlaced(Player* player, Placer const& placer, StoneAt const& stone);
/// Right-click: the selection menu (or Edit when sneaking). Creates a missing record first.
void onUsed(Player& player, StoneAt const& stone, bool sneaking);
/// The block is gone: removes its record.
void onRemoved(StoneAt const& stone);

/// The record uid of the sharestone at `stone`, creating the record if it has none (blocks set by
/// commands / structures). "" when no world is loaded.
std::string ensureRecord(StoneAt const& stone);

} // namespace bs::feature::sharestone
