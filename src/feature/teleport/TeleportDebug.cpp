// `/waystones debug teleport setup|go|missing|clear` (ops): two waystone records next to the
// player and 24 blocks north of it, used by the client e2e scenario `teleport` to warp the real
// player and screenshot the arrival and an action-bar error.
//
// setup: "Teleport A" 2 blocks east of the player (facing west, towards it) and "Teleport B" 24
//        blocks north (facing north, so the player lands north of it looking north at a gold
//        marker). The waystone block is placed when registered, else a lodestone stand-in.
// go:    warps from A (a waystone source) to B.
// missing: warps to a waystone that does not exist (error in the action bar).

#include "ll/api/chrono/GameChrono.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"

#include "feature/teleport/Teleport.h"
#include "game/Blocks.h"
#include "game/Ids.h"
#include "game/Waystones.h"
#include "mod/Commands.h"
#include "mod/Log.h"
#include "service/Services.h"

namespace bs::feature::teleport {

namespace {

std::string gA, gB;
std::vector<core::GlobalPos> gBlocks;

void clearAll() {
    auto& ws = game::Waystones::get();
    if (!gA.empty()) ws.remove(gA);
    if (!gB.empty()) ws.remove(gB);
    gA.clear();
    gB.clear();
    for (auto const& p : gBlocks) {
        if (auto* region = regionOf(p.dim)) game::clearBlock(*region, game::blockPos(p));
    }
    gBlocks.clear();
}

void put(BlockSource& region, core::GlobalPos const& pos, std::string_view id, int facing = 0) {
    auto const* block = game::blockWith(id);
    if (!block) return;
    if (ids::pathOf(id).ends_with("waystone")) {
        game::placeTwoTall(region, game::blockPos(pos), id, facing);
        gBlocks.push_back({pos.dim, pos.x, pos.y + 1, pos.z});
    } else {
        game::setBlock(region, game::blockPos(pos), *block);
    }
    gBlocks.push_back(pos);
}

std::string makeWaystone(BlockSource& region, std::string const& name, core::GlobalPos pos, int facing) {
    // Clear the landing spots so the arrival is deterministic.
    for (int dy = 0; dy < 3; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dz = -1; dz <= 1; ++dz) game::clearBlock(region, BlockPos{pos.x + dx, pos.y + dy, pos.z + dz});
        }
    }
    core::WaystoneRecord r;
    r.name     = name;
    r.pos      = pos;
    r.style    = "waystone";
    r.facing   = facing;
    r.origin   = core::Origin::Player;
    auto* rec  = game::Waystones::get().create(r);
    if (game::blockWith(ids::id("waystone"))) {
        put(region, pos, ids::id("waystone"), facing);
    } else {
        put(region, pos, "minecraft:lodestone");
        put(region, {pos.dim, pos.x, pos.y + 1, pos.z}, "minecraft:lodestone");
    }
    return rec ? rec->uid : std::string{};
}

} // namespace

BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload().text("debug").text("teleport").text("setup").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = commands::playerOf(origin);
        if (!player || !commands::isAdmin(origin)) return output.error("Only operators (players) can use this.");
        clearAll();
        auto& region = player->getDimensionBlockSource();
        auto  feet   = player->getFeetBlockPos();
        int   dim    = static_cast<int>(player->getDimensionId());
        gA           = makeWaystone(region, "Teleport A", {dim, feet.x + 2, feet.y, feet.z}, core::teleport::kWest);
        gB           = makeWaystone(region, "Teleport B", {dim, feet.x, feet.y, feet.z - 24}, core::teleport::kNorth);
        // A marker straight ahead of the arrival spot (north) shows the arrival direction.
        put(region, {dim, feet.x, feet.y + 1, feet.z - 30}, "minecraft:gold_block");
        log().info("teleport debug: setup A={} B={}", gA, gB);
        output.success("teleport debug: setup done");
    });
    cmd.overload().text("debug").text("teleport").text("go").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = commands::playerOf(origin);
        if (!player || !commands::isAdmin(origin) || gA.empty()) return output.error("Run /waystones debug teleport setup first.");
        service::WarpRequest req;
        req.source.kind    = core::WarpSourceKind::Waystone;
        req.source.fromUid = gA;
        if (auto* a = game::Waystones::get().find(gA)) req.source.fromPos = a->pos;
        req.targetId = gB;
        auto res     = service::get<service::ITeleport>().warp(*player, req);
        log().info("teleport debug: go -> {}", res.ok ? "ok" : res.error);
        auto id = player->getOrCreateUniqueID();
        ll::coro::keepThis([id]() -> ll::coro::CoroTask<> {
            co_await ll::chrono::ticks(10);
            auto lv = ll::service::getLevel();
            if (!lv) co_return;
            if (auto* p = lv->getPlayer(id)) {
                auto f = p->getFeetPos();
                log().info("teleport debug: at {:.1f} {:.1f} {:.1f} yaw {:.0f}", f.x, f.y, f.z, p->getRotation().y);
            }
        }).launch(ll::thread::ServerThreadExecutor::getDefault());
        output.success(res.ok ? "teleport debug: warped" : "teleport debug: refused");
    });
    cmd.overload().text("debug").text("teleport").text("missing").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = commands::playerOf(origin);
        if (!player || !commands::isAdmin(origin)) return output.error("Only operators (players) can use this.");
        service::WarpRequest req;
        req.source.kind = core::WarpSourceKind::Command;
        req.targetId    = "00000000-0000-4000-8000-000000000000";
        auto res        = service::get<service::ITeleport>().warp(*player, req);
        log().info("teleport debug: missing -> {}", res.ok ? "ok" : res.error);
        output.success("teleport debug: done");
    });
    cmd.overload().text("debug").text("teleport").text("clear").execute([](CommandOrigin const& origin, CommandOutput& output) {
        if (!commands::isAdmin(origin)) return output.error("Operators only.");
        clearAll();
        output.success("teleport debug: cleared");
    });
});

} // namespace bs::feature::teleport
