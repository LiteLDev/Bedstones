#include "core/portal/Placement.h"

#include <cmath>

namespace bs::core::portal {

std::optional<Cell> findPortalSpot(
    double               px,
    double               py,
    double               pz,
    double               lookX,
    double               lookZ,
    CellPredicate const& solid,
    CellPredicate const& replaceable
) {
    double len = std::sqrt(lookX * lookX + lookZ * lookZ);
    if (len < 1e-6) return std::nullopt;
    double nx = lookX / len, nz = lookZ / len;
    int    startY = static_cast<int>(std::floor(py + 2.0));
    for (int distance = 3; distance <= 4; ++distance) {
        int x = static_cast<int>(std::floor(px + nx * distance));
        int z = static_cast<int>(std::floor(pz + nz * distance));
        for (int y = startY; y >= startY - 5; --y) {
            if (solid(x, y - 1, z) && replaceable(x, y, z) && replaceable(x, y + 1, z) && replaceable(x, y + 2, z)) {
                return Cell{x, y, z};
            }
        }
    }
    return std::nullopt;
}

std::optional<Cell> findMemorialSpot(Cell death, CellPredicate const& replaceable) {
    Cell above{death.x, death.y + 1, death.z};
    if (replaceable(above.x, above.y, above.z)) return above;
    // Square layers around the death cell, x fastest then z (Java's BlockPos.betweenClosed order).
    for (int dy = 0; dy <= 3; ++dy) {
        for (int r = 1; r <= 2; ++r) {
            for (int dz = -r; dz <= r; ++dz) {
                for (int dx = -r; dx <= r; ++dx) {
                    Cell c{death.x + dx, death.y + dy, death.z + dz};
                    if (replaceable(c.x, c.y, c.z)) return c;
                }
            }
        }
    }
    return std::nullopt;
}

} // namespace bs::core::portal
