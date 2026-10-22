#pragma once

// Placement searches of the Warp Portal (Portal Scroll) and the Fleeting Memorial (Epitaph),
// docs/SPEC.md §1.6 / §1.7. Pure logic: the world is queried through predicates.

#include <functional>
#include <optional>

namespace bs::core::portal {

struct Cell {
    int x = 0, y = 0, z = 0;
    friend bool operator==(Cell const&, Cell const&) = default;
};

/// World query on a block cell.
using CellPredicate = std::function<bool(int x, int y, int z)>;

/// Ticks a Warp Portal stays open (5 minutes).
inline constexpr int kPortalLifetimeTicks = 6000;
/// Ticks before the same entity can use a portal again.
inline constexpr int kPortalReuseCooldownTicks = 20;

/// Where a Warp Portal opens for a player at feet position (px, py, pz) looking along the
/// horizontal direction (lookX, lookZ): 3, then 4 blocks ahead, scanning 5 cells down from y+2
/// for a solid block with 3 replaceable cells above it. Returns the lower portal cell.
[[nodiscard]] std::optional<Cell> findPortalSpot(
    double               px,
    double               py,
    double               pz,
    double               lookX,
    double               lookZ,
    CellPredicate const& solid,
    CellPredicate const& replaceable
);

/// Where a Fleeting Memorial goes for a player who died in cell `death`: the cell above it if
/// replaceable, otherwise the first replaceable cell at dy 0..3 within horizontal radius 1..2
/// around that cell.
[[nodiscard]] std::optional<Cell> findMemorialSpot(Cell death, CellPredicate const& replaceable);

} // namespace bs::core::portal
