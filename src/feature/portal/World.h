#pragma once

// Small world helpers shared by the portal and memorial features (server thread).

#include <utility>
#include <vector>

#include "ll/api/service/Bedrock.h"

#include "mc/world/actor/Actor.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/phys/AABB.h"

#include "core/Waystone.h"
#include "game/Blocks.h"
#include "service/Services.h"

namespace bs::feature::portal {

/// The main block source of dimension `dim` (nullptr when it doesn't exist / no level).
inline BlockSource* regionFor(int dim) {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    auto dimension = level->getDimension(dim).lock();
    if (!dimension) return nullptr;
    return &dimension->getBlockSourceFromMainChunkSource();
}

/// Entities whose hitbox intersects `box`, copied (safe to teleport / remove while iterating).
/// (`BlockSource::getEntities` misses players; the game's own query does not.)
inline std::vector<Actor*> entitiesIn(BlockSource& region, AABB const& box) {
    std::vector<Actor*> out;
    for (auto actor : region.fetchEntities(nullptr, box, true, false)) out.push_back(actor.get());
    return out;
}

/// Where a warp to record `r` lands (docs/SPEC.md §3.5): the cell on its facing side when that
/// cell and the one above are free, otherwise east, west, south, north, otherwise the record's own
/// cell. Centered, yaw = the facing.
inline service::Destination landingNear(core::WaystoneRecord const& r) {
    std::pair<int, int> cell{r.pos.x, r.pos.z};
    if (auto* region = regionFor(r.pos.dim)) {
        auto free = [&](int x, int z) {
            return game::replaceable(*region, BlockPos{x, r.pos.y, z}) && game::replaceable(*region, BlockPos{x, r.pos.y + 1, z});
        };
        auto [fx, fz] = game::facingOffset(r.facing);
        std::pair<int, int> const order[] = {{fx, fz}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (auto [dx, dz] : order) {
            if (free(r.pos.x + dx, r.pos.z + dz)) {
                cell = {r.pos.x + dx, r.pos.z + dz};
                break;
            }
        }
    }
    service::Destination d;
    d.block     = r.pos;
    d.x         = static_cast<float>(cell.first) + 0.5f;
    d.y         = static_cast<float>(r.pos.y);
    d.z         = static_cast<float>(cell.second) + 0.5f;
    d.yaw       = static_cast<float>(r.facing) * 90.f;
    d.targetUid = r.uid;
    return d;
}

} // namespace bs::feature::portal
