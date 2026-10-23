// `/waystones visuals ...` (op): debug helpers for the client visuals scenario.
//   place <kind>   a two-tall stone 3 blocks in front of the player plus its record. <kind> is a
//                  waystone style ("waystone", "blackstone_waystone", ...) or "<color>_sharestone".
//                  Uses the real `bedstones:<kind>` block when registered, else a vanilla
//                  stand-in (two stone brick walls, narrower than the rune band so runes show).
//   activate       activates every debug stone within 16 blocks for the player (record API)
//   forget         forgets them again
//   clear          removes every debug stone of the player's dimension (records and blocks)

#include <cmath>
#include <string>

#include "ll/api/command/CommandHandle.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"

#include "core/visuals/Visuals.h"
#include "game/Blocks.h"
#include "game/Ids.h"
#include "game/Waystones.h"
#include "mod/Commands.h"
#include "mod/I18n.h"
#include "mod/Log.h"

namespace bs::feature::visuals {

// Command parameter types need external linkage.
struct VisualsPlaceParams {
    std::string kind;
};

namespace {

constexpr char const* kDebugKey  = "visualsDebug";
constexpr char const* kStandIn   = "minecraft:stone_brick_wall";
constexpr double      kNearRange = 16.0;

bool validKind(std::string const& kind) {
    if (ids::isWaystoneStyle(kind)) return true;
    return core::types::isSharestone(kind); // there is no white sharestone
}

std::vector<core::WaystoneRecord*> debugStonesNear(Player& player, double range) {
    auto pos = player.getPosition();
    int  dim = static_cast<int>(player.getDimensionId());
    return game::Waystones::get().all([&](core::WaystoneRecord const& r) {
        return r.extra.value(kDebugKey, false) && (range <= 0 || core::visuals::inRange(r.pos, dim, pos.x, pos.y, pos.z, range));
    });
}

void place(Player& player, std::string const& kind, CommandOutput& output) {
    auto& ws = game::Waystones::get();
    if (!ws.available()) return output.error("no world");
    int  look   = game::playerFacing(player);
    auto [ox, oz] = game::facingOffset(look);
    auto feet   = player.getFeetPos();
    BlockPos pos{
        static_cast<int>(std::floor(feet.x)) + ox * 3,
        static_cast<int>(std::floor(feet.y + 0.01f)),
        static_cast<int>(std::floor(feet.z)) + oz * 3
    };
    auto& region = player.getDimensionBlockSource();
    int   facing = game::opposite(look);
    auto  id     = ids::id(kind);
    bool  real   = game::blockWith(id) != nullptr;
    bool  placed = false;
    if (real) {
        placed = game::placeTwoTall(region, pos, id, facing);
    } else if (auto const* wall = game::blockWith(kStandIn)) {
        BlockPos up{pos.x, pos.y + 1, pos.z};
        placed = game::replaceable(region, pos) && game::replaceable(region, up) && game::setBlock(region, pos, *wall)
              && game::setBlock(region, up, *wall);
    }
    if (!placed) {
        return output.error(trFor(player, "bs.visuals.debug.cannot_place", "Cannot place it there (two free blocks are needed 3 blocks ahead)."));
    }

    core::WaystoneRecord r;
    r.style  = kind;
    r.type   = ids::isWaystoneStyle(kind) ? std::string(core::types::kWaystone) : kind;
    r.name   = "Visuals " + kind;
    r.pos    = game::globalPos(region, pos);
    r.facing = facing;
    r.origin = core::Origin::Player;
    r.ownerUid  = game::Waystones::playerKey(player);
    r.ownerName = player.getRealName();
    r.visibility = core::types::isSharestone(r.type) ? core::Visibility::Sharestone : core::Visibility::Activation;
    r.extra[kDebugKey] = true;
    auto* rec = ws.create(std::move(r));
    if (!rec) {
        game::clearBlock(region, BlockPos{pos.x, pos.y + 1, pos.z});
        game::clearBlock(region, pos);
        return output.error("could not create the record");
    }
    log().info(
        "visuals: placed {} at {} {} {} ({}) uid {}",
        kind,
        pos.x,
        pos.y,
        pos.z,
        real ? "block" : "stand-in",
        rec->uid
    );
    auto how = real ? trFor(player, "bs.visuals.debug.block", "block") : trFor(player, "bs.visuals.debug.stand_in", "stand-in");
    output.success(trFor(player, "bs.visuals.debug.placed", "Placed {0} ({1}) with uid {2}", kind, how, rec->uid));
}

void setActivated(Player& player, bool on, CommandOutput& output) {
    auto& ws    = game::Waystones::get();
    int   count = 0;
    for (auto* r : debugStonesNear(player, kNearRange)) {
        if (!r->isWaystone()) continue;
        auto uid = r->uid;
        if (on ? ws.activate(player, uid) : ws.forget(player, uid)) ++count;
    }
    log().info("visuals: {} {} waystones for {}", on ? "activated" : "forgot", count, player.getRealName());
    output.success(
        on ? trFor(player, "bs.visuals.debug.activated", "Activated {0} waystone(s).", count)
           : trFor(player, "bs.visuals.debug.forgot", "Forgot {0} waystone(s).", count)
    );
}

void clear(Player& player, CommandOutput& output) {
    auto& ws     = game::Waystones::get();
    auto& region = player.getDimensionBlockSource();
    int   count  = 0;
    std::vector<std::string> uids;
    for (auto* r : debugStonesNear(player, 0)) {
        // Only stones of this dimension: their blocks are reachable through this region.
        if (r->pos.dim != static_cast<int>(region.getDimensionId())) continue;
        auto bp = game::blockPos(r->pos);
        game::clearBlock(region, BlockPos{bp.x, bp.y + 1, bp.z});
        game::clearBlock(region, bp);
        uids.push_back(r->uid);
    }
    for (auto const& uid : uids) {
        ws.remove(uid);
        ++count;
    }
    log().info("visuals: cleared {} debug stones", count);
    output.success(trFor(player, "bs.visuals.debug.cleared", "Removed {0} debug stone(s).", count));
}

} // namespace

BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload<VisualsPlaceParams>().text("visuals").text("place").required("kind").execute(
        [](CommandOrigin const& origin, CommandOutput& output, VisualsPlaceParams const& params) {
            if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
            auto* player = commands::playerOf(origin);
            if (!player) return output.error("Only players can place debug stones.");
            if (!validKind(params.kind)) {
                return output.error(trFor(
                    *player,
                    "bs.visuals.debug.unknown_kind",
                    "Unknown kind {0}: use a waystone style or <color>_sharestone.",
                    params.kind
                ));
            }
            place(*player, params.kind, output);
        }
    );
    auto simple = [&cmd](char const* name, auto fn) {
        cmd.overload().text("visuals").text(name).execute([fn](CommandOrigin const& origin, CommandOutput& output) {
            if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
            auto* player = commands::playerOf(origin);
            if (!player) return output.error("Only players can use this.");
            fn(*player, output);
        });
    };
    simple("activate", [](Player& p, CommandOutput& o) { setActivated(p, true, o); });
    simple("forget", [](Player& p, CommandOutput& o) { setActivated(p, false, o); });
    simple("clear", [](Player& p, CommandOutput& o) { clear(p, o); });
});

} // namespace bs::feature::visuals
