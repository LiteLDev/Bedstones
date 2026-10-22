#pragma once

// Epitaph + Fleeting Memorial (docs/SPEC.md §1.7, §2, §3.7).
//
// A player dying with an Epitaph anywhere in the inventory leaves a Fleeting Memorial (one epitaph
// is used up). The memorial is a `fleeting_memorial` record owned by that player, listed pinned in
// the owner's selection menus only, and destroyed after the owner warps to it. Its owner's name
// floats above it on an invisible `bedstones:memorial_label` entity.

#include <string>

#include "core/portal/Placement.h"

class Player;

namespace bs::feature::memorial {

/// Entity type of the floating owner-name label.
inline constexpr char const* kLabelEntity = "bedstones:memorial_label";

/// Handles a player's death: if an epitaph is carried, places a memorial and uses one epitaph.
/// Returns the memorial uid ("" when nothing happened).
std::string onPlayerDeath(Player& player);

/// Places a memorial block at `cell` (dimension `dim`) owned by `owner` and creates its record.
/// Returns the record uid ("" on failure).
std::string spawnMemorial(Player const& owner, int dim, core::portal::Cell cell, int facing);
/// Removes the memorial block, its label and its record.
void removeMemorial(std::string const& uid);

/// Number of label entities within 2 blocks of the memorial (selftests).
[[nodiscard]] int labelCount(std::string const& uid);

} // namespace bs::feature::memorial
