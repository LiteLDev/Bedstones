#pragma once

// Selftest helpers of U5 (portal.*, memorial.*): a stone floor in the air far from the spawn, a
// player standing on it looking east, and waystone targets. Test x offsets: 500..599.

#include <optional>
#include <string>

#include "ll/api/coro/CoroTask.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionType.h"

#include "game/Blocks.h"
#include "game/Waystones.h"
#include "selftest/SelfTest.h"

namespace bs::feature::portal::test {

/// Height of the test floors (in the air above usual terrain; the spawn y of flat worlds is not
/// a usable height).
inline constexpr int kFloorY = 150;

inline BlockSource* overworld() {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    auto dim = level->getDimension(0).lock();
    return dim ? &dim->getBlockSourceFromMainChunkSource() : nullptr;
}

/// A stone floor whose top is at `origin.y` (the floor blocks are at y - 1), `dx0..dx1` x `dz0..dz1`.
inline void floor(selftest::Context& ctx, core::GlobalPos origin, int dx0, int dx1, int dz0, int dz1) {
    for (int x = dx0; x <= dx1; ++x) {
        for (int z = dz0; z <= dz1; ++z) ctx.setBlock({0, origin.x + x, origin.y - 1, origin.z + z}, "minecraft:stone");
    }
}

struct Floor {
    int dx0 = 0, dx1 = 0, dz0 = 0, dz1 = 0;
};

/// Puts `player` at `feet` facing `yaw` (0 south, -90 east), waits until the chunk is loaded and
/// builds the floor (if any) under it.
inline ll::coro::CoroTask<bool>
place(selftest::Context& ctx, Player& player, core::GlobalPos feet, std::optional<Floor> fl = std::nullopt, float yaw = -90.f) {
    Vec3 at{static_cast<float>(feet.x) + 0.5f, static_cast<float>(feet.y), static_cast<float>(feet.z) + 0.5f};
    player.teleport(at, DimensionType(0), Vec2{0.f, yaw});
    for (int i = 0; i < 100; ++i) {
        co_await ctx.ticks(1);
        auto* region = overworld();
        if (region && region->hasChunksAt(game::blockPos(feet), 24, false)) {
            if (fl) floor(ctx, feet, fl->dx0, fl->dx1, fl->dz0, fl->dz1);
            player.teleport(at, DimensionType(0), Vec2{0.f, yaw});
            co_await ctx.ticks(2);
            co_return true;
        }
    }
    co_return false;
}

/// The feet position of a test area at `dx` (in the air, above a floor once `floor` is called).
inline core::GlobalPos arena(selftest::Context& ctx, int dx, int dz = 0) {
    auto p = ctx.testPos(dx, dz);
    p.y    = kFloorY;
    return p;
}

/// A named waystone record at `pos` (plus the block when the waystone block exists), activated
/// for `player`.
inline core::WaystoneRecord& target(selftest::Context& ctx, Player& player, core::GlobalPos pos, std::string name) {
    core::WaystoneRecord r;
    r.name   = std::move(name);
    r.pos    = pos;
    r.style  = "waystone";
    r.facing = 0;
    r.origin = core::Origin::Player;
    if (game::blockWith("bedstones:waystone")) {
        ctx.setBlock(pos, "bedstones:waystone", {{"bedstones:facing", 0}, {"bedstones:upper", 0}});
        ctx.setBlock({pos.dim, pos.x, pos.y + 1, pos.z}, "bedstones:waystone", {{"bedstones:facing", 0}, {"bedstones:upper", 1}});
    }
    auto& rec = ctx.createWaystone(std::move(r));
    game::Waystones::get().activate(player, rec.uid);
    return rec;
}

} // namespace bs::feature::portal::test
