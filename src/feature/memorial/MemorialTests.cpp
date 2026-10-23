// Selftests of the Epitaph / Fleeting Memorial (`/waystones selftest "memorial.*"`) and the
// `/waystones debug memorials` command used by the client e2e scenario.

#include <cmath>

#include "ll/api/command/CommandHandle.h"

#include "mc/server/SimulatedPlayer.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/GameType.h"

#include "feature/memorial/Memorial.h"
#include "feature/portal/TestArena.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "mod/Commands.h"
#include "mod/Log.h"
#include "service/Services.h"

namespace {

using bs::selftest::Context;
namespace memorial = bs::feature::memorial;
namespace arena    = bs::feature::portal::test;

std::string const kEpitaph  = "bedstones:epitaph";
std::string const kMemorial = "bedstones:fleeting_memorial";

std::string blockAt(bs::core::GlobalPos p) {
    auto* region = arena::overworld();
    return region ? bs::game::blockIdAt(*region, bs::game::blockPos(p)) : std::string{};
}

int countOf(Player& player, std::string const& id) {
    int   n   = 0;
    auto& inv = player.getInventory();
    for (int i = 0; i < inv.getContainerSize(); ++i) {
        auto const& s = inv.getItem(i);
        if (bs::game::idOf(s) == id) n += s.mCount;
    }
    return n;
}

std::vector<bs::core::WaystoneRecord*> memorialsOf(Player& player) {
    auto key = bs::game::Waystones::playerKey(player);
    return bs::game::Waystones::get().all([&](auto const& r) {
        return r.type == bs::core::types::kFleetingMemorial && r.ownerUid == key;
    });
}

bool listed(Player& player, std::string const& uid, bs::core::WarpSourceKind kind = bs::core::WarpSourceKind::WarpScroll) {
    bs::core::WarpSource src;
    src.kind = kind;
    for (auto const& t : bs::service::get<bs::service::ITargets>().list(player, src)) {
        if (t.id == uid) return t.pinned && t.kind == "fleeting_memorial";
    }
    return false;
}

} // namespace

// Death handling with an epitaph: memorial above the death cell, one epitaph used, record owned
// by the player (called directly on a living player so the inventory can be checked).
BS_REGISTER_SELFTEST("memorial.epitaph_death_handler", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p    = ctx.player();
    auto  feet = arena::arena(ctx, 510);
    ctx.require(co_await arena::place(ctx, p, feet, arena::Floor{-2, 2, -2, 2}), "test area loaded");
    ctx.give(p, bs::game::makeItem(kEpitaph, 1), 20);
    auto uid = memorial::onPlayerDeath(p);
    ctx.require(!uid.empty(), "memorial placed");
    ctx.cleanupWaystone(uid);
    auto* r = bs::game::Waystones::get().find(uid);
    ctx.require(r != nullptr, "memorial record");
    ctx.cleanupBlock(r->pos);
    ctx.check(r->pos == bs::core::GlobalPos{0, feet.x, feet.y + 1, feet.z}, "placed above the death cell");
    ctx.check(r->type == "fleeting_memorial" && r->name == "Fleeting Memorial", "record type and name");
    ctx.check(r->ownerUid == bs::game::Waystones::playerKey(p) && r->ownerName == p.getRealName(), "owned by the dead player");
    ctx.check(r->visibility == bs::core::Visibility::Activation, "activation visibility");
    ctx.check(blockAt(r->pos) == kMemorial, "memorial block placed");
    ctx.check(countOf(p, kEpitaph) == 0, "epitaph used up");
    ctx.check(memorial::onPlayerDeath(p).empty(), "no epitaph, no memorial");
    co_await ctx.ticks(25);
    ctx.check(memorial::labelCount(uid) >= 1, "owner name label present");
});

// A real death of a simulated player carrying an epitaph leaves a memorial.
BS_REGISTER_SELFTEST("memorial.epitaph_death", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p    = ctx.spawnPlayer();
    auto  feet = arena::arena(ctx, 530);
    ctx.require(co_await arena::place(ctx, p, feet, arena::Floor{-2, 2, -2, 2}), "test area loaded");
    p.setPlayerGameType(GameType::Survival);
    ctx.give(p, bs::game::makeItem(kEpitaph, 1));
    p.kill();
    co_await ctx.ticks(3);
    auto list = memorialsOf(p);
    ctx.require(list.size() == 1, "one memorial after death");
    ctx.cleanupWaystone(list.front()->uid);
    ctx.cleanupBlock(list.front()->pos);
    ctx.check(blockAt(list.front()->pos) == kMemorial, "memorial block placed");
    ctx.check(std::abs(list.front()->pos.x - feet.x) <= 2 && std::abs(list.front()->pos.z - feet.z) <= 2, "near the death position");
});

// Listed for its owner only (pinned, for warp scrolls but not portstones); warping there removes it.
BS_REGISTER_SELFTEST("memorial.list_and_warp", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& owner = ctx.player();
    auto& other = ctx.spawnPlayer();
    auto  feet  = arena::arena(ctx, 550);
    ctx.require(co_await arena::place(ctx, owner, feet, arena::Floor{-3, 8, -2, 2}), "test area loaded");
    co_await arena::place(ctx, other, feet);
    owner.setPlayerGameType(GameType::Survival);
    owner.addLevels(30);
    bs::core::GlobalPos at{0, feet.x + 6, feet.y, feet.z};
    auto                uid = memorial::spawnMemorial(owner, 0, {at.x, at.y, at.z}, 1);
    ctx.require(!uid.empty(), "memorial spawned");
    ctx.cleanupWaystone(uid);
    ctx.cleanupBlock(at);
    ctx.check(listed(owner, uid), "listed for the owner (warp scroll)");
    ctx.check(listed(owner, uid, bs::core::WarpSourceKind::PortalScroll), "listed for the owner (portal scroll)");
    ctx.check(!listed(owner, uid, bs::core::WarpSourceKind::Portstone), "not listed for portstones");
    ctx.check(!listed(other, uid), "not listed for other players");
    ctx.check(!bs::service::get<bs::service::ITeleport>().resolve(other, uid), "other players cannot warp to it");
    bs::service::WarpRequest req;
    req.source.kind = bs::core::WarpSourceKind::WarpScroll;
    req.targetId    = uid;
    auto res        = bs::service::get<bs::service::ITeleport>().warp(owner, req);
    ctx.require(res.ok, "owner warps to the memorial: " + res.error);
    co_await ctx.ticks(2);
    ctx.check(bs::game::Waystones::get().find(uid) == nullptr, "memorial record removed after the warp");
    ctx.check(blockAt(at) != kMemorial, "memorial block removed after the warp");
    auto landed = owner.getPosition();
    ctx.check(
        std::abs(landed.x - (static_cast<float>(at.x) - 0.5f)) < 0.6f && std::abs(landed.z - (static_cast<float>(at.z) + 0.5f)) < 0.6f,
        "landed in front of it (at " + std::to_string(landed.x) + " " + std::to_string(landed.y) + " " + std::to_string(landed.z) + ")"
    );
});

// A memorial broken by other means loses its record.
BS_REGISTER_SELFTEST("memorial.broken", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p    = ctx.player();
    auto  feet = arena::arena(ctx, 570);
    ctx.require(co_await arena::place(ctx, p, feet, arena::Floor{-2, 2, -2, 2}), "test area loaded");
    bs::core::GlobalPos at{0, feet.x + 1, feet.y, feet.z};
    auto                uid = memorial::spawnMemorial(p, 0, {at.x, at.y, at.z}, 0);
    ctx.require(!uid.empty(), "memorial spawned");
    ctx.cleanupWaystone(uid);
    ctx.cleanupBlock(at);
    bs::game::clearBlock(*arena::overworld(), bs::game::blockPos(at));
    co_await ctx.ticks(25);
    ctx.check(bs::game::Waystones::get().find(uid) == nullptr, "record removed with the block");
    ctx.check(memorial::labelCount(uid) == 0, "label removed");
});

// `/waystones debug memorials`: logs the caller's memorials and opens a warp-scroll selection menu.
BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload().text("debug").text("memorials").execute([](CommandOrigin const& origin, CommandOutput& output) {
        if (!bs::commands::isAdmin(origin)) return output.error("You need operator permission.");
        auto* player = bs::commands::playerOf(origin);
        if (!player) return output.error("Only players can open the selection menu.");
        bs::core::WarpSource src;
        src.kind  = bs::core::WarpSourceKind::WarpScroll;
        int count = 0;
        for (auto const& t : bs::service::get<bs::service::ITargets>().list(*player, src)) {
            if (t.kind != "fleeting_memorial") continue;
            ++count;
            bs::log().info("memorial debug: listed {} ({}) at {} {} {}", t.name, t.id, t.pos.x, t.pos.y, t.pos.z);
        }
        bs::log().info("memorial debug: {} memorial(s) listed for {}", count, player->getRealName());
        bs::service::get<bs::service::ISelection>().open(*player, src);
        output.success(std::to_string(count) + " memorial(s) listed");
    });
});
