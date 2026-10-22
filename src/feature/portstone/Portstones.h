#pragma once

// Portstone behaviour (docs/SPEC.md §1.4): no record; right-click (sneaking or not) opens the
// selection menu with a Portstone source of the block's colour.

#include "feature/sharestone/StoneBlocks.h"

namespace bs::feature::portstone {

using sharestone::Placer;
using sharestone::StoneAt;

void onPlaced(Placer const& placer, StoneAt const& stone);
void onUsed(Player& player, StoneAt const& stone);

} // namespace bs::feature::portstone
