#pragma once

// Small world helpers shared by the portal and memorial features (server thread).

#include <vector>

#include "ll/api/service/Bedrock.h"

#include "mc/world/actor/Actor.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/phys/AABB.h"

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

} // namespace bs::feature::portal
