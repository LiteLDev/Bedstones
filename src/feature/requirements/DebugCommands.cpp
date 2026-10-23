// U8 debug helpers for e2e runs (op only):
//   /waystones debug requirements warp <distance> [source]   warps `distance` blocks east through
//       ITeleport with the given source kind (default "waystone"), paying the configured costs
//   /waystones debug requirements show                       prints XP and active cooldowns
//   /waystones debug requirements reset                      clears the player's cooldowns

#include <cmath>

#include "ll/api/chrono/GameChrono.h"
#include "ll/api/command/CommandHandle.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/thread/ServerThreadExecutor.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"

#include "ll/api/service/Bedrock.h"

#include "mc/world/level/BlockPos.h"
#include "mc/world/level/Level.h"

#include "feature/requirements/Requirements.h"
#include "feature/teleport/Teleport.h"
#include "game/Blocks.h"
#include "game/Waystones.h"
#include "mod/Commands.h"
#include "mod/Log.h"
#include "service/Services.h"

namespace bs::feature::requirements {

// Command parameter types need external linkage.
struct DebugWarpParams {
    int         distance = 0;
    std::string source;
};

namespace {

void debugWarp(CommandOrigin const& origin, CommandOutput& output, int distance, std::string const& sourceName) {
    if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
    auto* player = commands::playerOf(origin);
    if (!player) return output.error("Only players can warp.");
    auto kind = core::warpSourceFromString(sourceName.empty() ? "waystone" : sourceName);
    if (!kind) return output.error("Unknown warp source: " + sourceName);

    auto&                at = player->getPosition();
    core::WaystoneRecord rec;
    rec.name   = "Requirements Debug";
    rec.style  = "waystone";
    rec.origin    = core::Origin::Player;
    rec.transient = true; // never listed, even if the world stops before it is removed
    rec.pos    = {static_cast<int>(player->getDimensionId()), static_cast<int>(std::floor(at.x)) + distance,
                  static_cast<int>(std::floor(at.y - 1.62f + 0.01f)), static_cast<int>(std::floor(at.z))};
    auto* created = game::Waystones::get().create(rec);
    if (!created) return output.error("No world loaded.");
    std::string uid = created->uid;

    core::WarpSource src;
    src.kind = *kind;
    core::WarpTarget target;
    target.id   = uid;
    target.kind = created->type;
    target.pos  = created->pos;
    auto cost   = service::get<service::IRequirements>().resolve(*player, src, target);
    // The target is far away: load its chunks and put a real waystone block there first (the
    // teleport checks that the target block exists), then warp.
    auto playerId = player->getOrCreateUniqueID();
    auto pos      = created->pos;
    ll::coro::keepThis([uid, src, cost, distance, playerId, pos, kind = *kind]() -> ll::coro::CoroTask<> {
        bool loaded = co_await feature::teleport::preloadDestination(pos);
        auto level  = ll::service::getLevel();
        auto* p     = level ? level->getPlayer(playerId) : nullptr;
        auto* rec   = game::Waystones::get().find(uid);
        if (!p || !rec) co_return;
        if (loaded) game::placeRecordBlock(*rec);
        auto before = totalXp(*p);
        auto res    = service::get<service::ITeleport>().warp(*p, {src, uid});
        log().info(
            "requirements debug warp: {} distance {} source {} cost {} xp -> {} (xp {} -> {})",
            p->getRealName(),
            distance,
            core::toString(kind),
            cost.xpPoints,
            res.ok ? "ok" : "failed: " + res.errorFallback,
            before,
            totalXp(*p)
        );
        // Keep the temporary target until the teleport is surely done.
        co_await ll::chrono::ticks(200);
        if (auto* r = game::Waystones::get().find(uid)) {
            auto at = r->pos;
            game::Waystones::get().remove(uid);
            if (auto* region = feature::teleport::regionOf(at.dim)) {
                game::clearBlock(*region, BlockPos{at.x, at.y + 1, at.z});
                game::clearBlock(*region, game::blockPos(at));
            }
        }
    }).launch(ll::thread::ServerThreadExecutor::getDefault());
    output.success("warping (cost " + std::to_string(cost.xpPoints) + " xp points)");
}

} // namespace

BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload<DebugWarpParams>()
        .text("debug")
        .text("requirements")
        .text("warp")
        .required("distance")
        .optional("source")
        .execute([](CommandOrigin const& origin, CommandOutput& output, DebugWarpParams const& p) {
            debugWarp(origin, output, p.distance, p.source);
        });
    cmd.overload().text("debug").text("requirements").text("show").execute([](CommandOrigin const& origin, CommandOutput& output) {
        if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
        auto* player = commands::playerOf(origin);
        if (!player) return output.error("Only players have experience.");
        std::string cds;
        for (auto const& [id, ms] : activeCooldowns(*player)) cds += " " + id + "=" + std::to_string(ms / 1000) + "s";
        auto line = "level " + std::to_string(playerLevel(*player)) + ", " + std::to_string(totalXp(*player)) + " xp points, cooldowns:"
                  + (cds.empty() ? std::string(" none") : cds);
        log().info("requirements debug: {} {}", player->getRealName(), line);
        output.success(line);
    });
    cmd.overload().text("debug").text("requirements").text("reset").execute([](CommandOrigin const& origin, CommandOutput& output) {
        if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
        auto* player = commands::playerOf(origin);
        if (!player) return output.error("Only players have cooldowns.");
        resetCooldowns(*player);
        log().info("requirements debug: cooldowns reset for {}", player->getRealName());
        output.success("cooldowns reset");
    });
});

} // namespace bs::feature::requirements
