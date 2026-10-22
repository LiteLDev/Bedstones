#pragma once

// Selftest helpers of the sharestone / portstone unit: a loaded test site next to the simulated
// player, and recording probes temporarily swapped into the ISelection / IEditScreen seams.

#include <memory>
#include <string>
#include <vector>

#include "ll/api/coro/CoroTask.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/server/SimulatedPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/dimension/DimensionType.h"

#include "core/Warp.h"
#include "game/Blocks.h"
#include "selftest/SelfTest.h"
#include "service/Services.h"

namespace bs::feature::sharestone::test {

inline SimulatedPlayer& asSim(Player& p) { return static_cast<SimulatedPlayer&>(p); }
inline BlockPos         above(BlockPos const& p, int n = 1) { return BlockPos{p.x, p.y + n, p.z}; }

/// Replaces seam `I` with `probe` for the lifetime of this object, then puts back whatever was
/// installed before (another unit's implementation or the default).
template <class I>
class SeamSwap {
public:
    explicit SeamSwap(std::unique_ptr<I> probe) {
        mOld                       = std::move(service::detail::slot<I>());
        service::detail::slot<I>() = std::move(probe);
    }
    ~SeamSwap() { service::detail::slot<I>() = std::move(mOld); }
    SeamSwap(SeamSwap const&)            = delete;
    SeamSwap& operator=(SeamSwap const&) = delete;

private:
    std::unique_ptr<I> mOld;
};

struct SelectionProbe final : service::ISelection {
    std::vector<core::WarpSource>* opened;
    explicit SelectionProbe(std::vector<core::WarpSource>* out) : opened(out) {}
    void open(Player&, core::WarpSource const& source) override { opened->push_back(source); }
    void openAdmin(Player&, std::string const&) override {}
};

struct EditProbe final : service::IEditScreen {
    std::vector<std::string>* opened;
    explicit EditProbe(std::vector<std::string>* out) : opened(out) {}
    void open(Player&, std::string const& uid) override { opened->push_back(uid); }
};

/// A free ground cell (the lower half of a stone goes there) `dx` blocks east of spawn. Moves the
/// player two blocks south of it, looking north, and waits until the chunks are loaded. Clears
/// the two cells above the ground and registers them (and the one above) for cleanup.
inline ll::coro::CoroTask<> prepareSite(selftest::Context& ctx, Player& player, int dx, BlockPos& out) {
    auto  base   = ctx.testPos(dx);
    auto& region = player.getDimensionBlockSource();
    player.teleport(
        Vec3{static_cast<float>(base.x) + 0.5f, 100.f, static_cast<float>(base.z) + 2.5f},
        DimensionType(0),
        Vec2{30.f, 180.f}
    );
    BlockPos probe{base.x, 0, base.z};
    for (int i = 0; i < 100 && !region.areChunksFullyLoaded(probe, 1); ++i) co_await ctx.ticks(2);
    ctx.require(region.areChunksFullyLoaded(probe, 1), "test site chunks loaded");
    int y = region.getAboveTopSolidBlock(base.x, base.z, false, false);
    out   = BlockPos{base.x, y, base.z};
    for (int dy = 0; dy < 3; ++dy) {
        core::GlobalPos cell{0, out.x, out.y + dy, out.z};
        game::clearBlock(region, game::blockPos(cell));
        ctx.cleanupBlock(cell);
    }
    player.teleport(
        Vec3{static_cast<float>(out.x) + 0.5f, static_cast<float>(out.y), static_cast<float>(out.z) + 2.5f},
        DimensionType(0),
        Vec2{30.f, 180.f}
    );
    co_await ctx.ticks(2);
}

} // namespace bs::feature::sharestone::test
