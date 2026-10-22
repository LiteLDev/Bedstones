// `/waystones debug feather partner|menu|clear`: e2e helpers for the Twinbound Feather.
//
// - partner: spawns a simulated player "Twin" 1.5 blocks in front of the caller, facing them,
//   holding a feather and holding use, so the caller can do the linking ritual alone.
// - menu: opens the selection menu as the inventory button would (lists Twinbound partners).
// - clear: removes the simulated partner.

#include <cmath>
#include <numbers>

#include "ll/api/command/CommandHandle.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/platform/UUID.h"
#include "mc/server/SimulatedPlayer.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include "feature/feather/Feather.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/Commands.h"
#include "mod/Log.h"
#include "service/Services.h"

namespace bs::feature::feather {

namespace {

std::string gPartnerUuid;

SimulatedPlayer* debugPartner() {
    auto level = ll::service::getLevel();
    if (!level || gPartnerUuid.empty()) return nullptr;
    auto* p = level->getPlayer(mce::UUID::fromString(gPartnerUuid));
    return p && p->isSimulatedPlayer() ? static_cast<SimulatedPlayer*>(p) : nullptr;
}

void spawnPartner(Player& caller, CommandOutput& output) {
    if (auto* old = debugPartner()) old->simulateDisconnect();
    gPartnerUuid.clear();
    auto  feet = caller.getFeetPos();
    float yaw  = caller.getRotation().y;
    float rad  = yaw * std::numbers::pi_v<float> / 180.f;
    Vec3  pos{feet.x - std::sin(rad) * 1.5f, feet.y, feet.z + std::cos(rad) * 1.5f};
    auto  sp = SimulatedPlayer::create("Twin", pos, caller.getDimensionId(), Vec2{0.f, yaw + 180.f});
    if (!sp) return output.error("could not spawn the simulated partner");
    gPartnerUuid = game::Waystones::playerKey(*sp);
    sp->simulateSetItem(game::makeItem(ids::id(ids::kTwinboundFeather)), true, 0);
    sp->simulateLookAt(caller, sim::LookDuration::Continuous);
    sp->simulateUseItem();
    bool vanilla = isUsingFeather(*sp);
    if (!vanilla) simulateUse(*sp, true);
    log().info("feather: debug partner {} spawned (use: {})", sp->getRealName(), vanilla ? "vanilla" : "simulated");
    output.success("Spawned Twin holding a Twinbound Feather");
}

} // namespace

BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload().text("debug").text("feather").text("partner").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = commands::playerOf(origin);
        if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
        if (!player) return output.error("Only players can use this.");
        spawnPartner(*player, output);
    });
    cmd.overload().text("debug").text("feather").text("menu").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = commands::playerOf(origin);
        if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
        if (!player) return output.error("Only players can use this.");
        core::WarpSource source;
        source.kind = core::WarpSourceKind::InventoryButton;
        auto partners = partnersOf(*player);
        log().info("feather: debug menu for {} ({} partner(s))", player->getRealName(), partners.size());
        service::get<service::ISelection>().open(*player, source);
        output.success("Opened the selection menu");
    });
    cmd.overload().text("debug").text("feather").text("clear").execute([](CommandOrigin const& origin, CommandOutput& output) {
        if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
        if (auto* p = debugPartner()) p->simulateDisconnect();
        gPartnerUuid.clear();
        output.success("Removed the simulated partner");
    });
});

} // namespace bs::feature::feather
