// `/waystones worldgen locate [x z]`    nearest wild waystone cell (testing)
// `/waystones worldgen testblock <id>`  debug stand-in for missing style blocks ("off" clears)

#include <cmath>
#include <optional>
#include <utility>

#include "ll/api/command/CommandHandle.h"
#include "ll/api/service/Bedrock.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"

#include "feature/worldgen/Worldgen.h"
#include "game/Blocks.h"
#include "game/Waystones.h"
#include "mod/Commands.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Log.h"

namespace bs::feature::worldgen {

// Command parameter types need external linkage.
struct WorldgenTestBlockParams {
    std::string block;
};
struct WorldgenLocateParams {
    int x = 0;
    int z = 0;
};

namespace {

namespace cw = core::worldgen;

template <class... Args>
std::string text(Player const* p, std::string_view key, std::string_view fallback, Args const&... args) {
    return p ? trFor(*p, key, fallback, args...) : tr(key, fallback, args...);
}

/// `at` = an explicit column (x, z); otherwise the player's position (the console uses 0, 0).
void locate(CommandOrigin const& origin, CommandOutput& output, std::optional<std::pair<int, int>> at) {
    if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
    auto* player = commands::playerOf(origin);
    int   dim    = player ? static_cast<int>(player->getDimensionId()) : 0;
    int   bx = 0, bz = 0;
    if (at) {
        bx = at->first;
        bz = at->second;
    } else if (player) {
        auto pos = player->getPosition();
        bx       = static_cast<int>(std::floor(pos.x));
        bz       = static_cast<int>(std::floor(pos.z));
    }
    int n = config::get<WorldGenConfig>().chunksBetweenWildWaystones;
    if (n <= 0) return output.error(text(player, "bs.worldgen.locate.disabled", "Wild waystones are disabled (chunksBetweenWildWaystones = 0)."));

    auto spot = cw::nearestSpot(worldSeed(), bx, bz, n);
    auto dist = static_cast<int>(std::lround(std::hypot(double(spot.blockX - bx), double(spot.blockZ - bz))));
    std::string status;
    std::string statusEn;
    if (!wildEnabledIn(dim)) {
        status   = text(player, "bs.worldgen.locate.status.disabled", "disabled in this dimension");
        statusEn = "disabled";
    } else if (isCellDone(cw::cellKey(dim, n, spot.cell))) {
        auto found = game::Waystones::get().all([&](core::WaystoneRecord const& r) {
            // generateWild may move a few columns away from a blocked spot.
            return r.origin == core::Origin::Wilderness && r.pos.dim == dim && std::abs(r.pos.x - spot.blockX) <= 3
                && std::abs(r.pos.z - spot.blockZ) <= 3;
        });
        if (!found.empty()) {
            status   = text(player, "bs.worldgen.locate.status.placed", "placed at y {}", found.front()->pos.y);
            statusEn = "placed y=" + std::to_string(found.front()->pos.y);
        } else {
            status   = text(player, "bs.worldgen.locate.status.skipped", "generated without a waystone (no space)");
            statusEn = "skipped";
        }
    } else {
        status   = text(player, "bs.worldgen.locate.status.pending", "not generated yet");
        statusEn = "pending";
    }
    log().info(
        "worldgen locate: nearest wild waystone at block {} {} (cell {} {}, chunk {} {}) in {}: {}",
        spot.blockX,
        spot.blockZ,
        spot.cell.x,
        spot.cell.z,
        spot.chunkX,
        spot.chunkZ,
        cw::dimensionName(dim),
        statusEn
    );
    output.success(text(
        player,
        "bs.worldgen.locate",
        "Nearest wild waystone: {} ~ {} ({} blocks away, chunk {}, {}): {}",
        spot.blockX,
        spot.blockZ,
        dist,
        spot.chunkX,
        spot.chunkZ,
        status
    ));
}

void testBlockCmd(CommandOrigin const& origin, CommandOutput& output, WorldgenTestBlockParams const& params) {
    if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
    auto* player = commands::playerOf(origin);
    if (params.block == "off" || params.block.empty()) {
        setTestBlock({});
        log().info("worldgen test block: off");
        return output.success(text(player, "bs.worldgen.testblock.off", "World generation uses the waystone blocks only."));
    }
    std::string id = params.block.find(':') == std::string::npos ? "minecraft:" + params.block : params.block;
    if (!game::blockWith(id)) return output.error(text(player, "bs.worldgen.testblock.unknown", "Unknown block: {}", id));
    setTestBlock(id);
    log().info("worldgen test block: {}", id);
    output.success(text(player, "bs.worldgen.testblock.set", "Missing waystone blocks are generated as {} (debug).", id));
}

} // namespace

BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload().text("worldgen").text("locate").execute([](CommandOrigin const& o, CommandOutput& out) {
        locate(o, out, std::nullopt);
    });
    cmd.overload<WorldgenLocateParams>().text("worldgen").text("locate").required("x").required("z").execute(
        [](CommandOrigin const& o, CommandOutput& out, WorldgenLocateParams const& p) {
            locate(o, out, std::pair{p.x, p.z});
        }
    );
    cmd.overload<WorldgenTestBlockParams>().text("worldgen").text("testblock").required("block").execute(
        [](CommandOrigin const& o, CommandOutput& out, WorldgenTestBlockParams const& p) { testBlockCmd(o, out, p); }
    );
});

} // namespace bs::feature::worldgen
