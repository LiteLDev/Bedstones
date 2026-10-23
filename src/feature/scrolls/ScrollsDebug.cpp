// `/waystones debug scrolls`: sets up two activated test waystones (records) next to the player
// for the scrolls e2e scenario, with real waystone blocks (a blank scroll can be used on A, and
// teleports find their target block). Admin only.

#include <cmath>

#include "ll/api/command/CommandHandle.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"

#include "game/Blocks.h"
#include "game/Waystones.h"
#include "mod/Commands.h"
#include "mod/Log.h"

namespace bs::feature::scrolls {

namespace {

std::string ensureRecord(Player& p, std::string const& name, core::GlobalPos pos) {
    auto& ws = game::Waystones::get();
    for (auto* r : ws.all()) {
        if (r->name == name) {
            ws.move(r->uid, pos);
            ws.activate(p, r->uid);
            return r->uid;
        }
    }
    core::WaystoneRecord r;
    r.name      = name;
    r.pos       = pos;
    r.style     = "waystone";
    r.facing    = 0; // land south of it
    r.origin    = core::Origin::Player;
    r.ownerUid  = game::Waystones::playerKey(p);
    r.ownerName = p.getRealName();
    auto* made  = ws.create(r);
    if (!made) return {};
    ws.activate(p, made->uid);
    return made->uid;
}

} // namespace

BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload().text("debug").text("scrolls").execute([](CommandOrigin const& origin, CommandOutput& output) {
        if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
        auto* p = commands::playerOf(origin);
        if (!p) return output.error("Only players can run this.");
        auto feet = p->getFeetPos();
        int  dim  = static_cast<int>(p->getDimensionId());
        int  x = static_cast<int>(std::floor(feet.x)), y = static_cast<int>(std::floor(feet.y)), z = static_cast<int>(std::floor(feet.z));
        core::GlobalPos a{dim, x + 3, y, z};
        core::GlobalPos b{dim, x - 12, y, z + 6};
        auto            aUid = ensureRecord(*p, "Scroll Test A", a);
        auto            bUid = ensureRecord(*p, "Scroll Test B", b);
        if (aUid.empty() || bUid.empty()) return output.error("no world");
        bool placed = false;
        for (auto const& uid : {aUid, bUid}) {
            if (auto* r = game::Waystones::get().find(uid)) placed = game::placeRecordBlock(*r) || placed;
        }
        log().info("scrolls debug: A={} at {} {} {}, B={} at {} {} {} (block placed: {})", aUid, a.x, a.y, a.z, bUid, b.x, b.y, b.z, placed);
        output.success("Scroll test waystones ready: A (" + std::to_string(a.x) + " " + std::to_string(a.y) + " " + std::to_string(a.z) + "), B");
    });
});

} // namespace bs::feature::scrolls
