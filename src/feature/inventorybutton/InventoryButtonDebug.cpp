// Debug helpers of the inventory button for e2e scenarios (operators only):
//   /waystones inventorybutton debug waystone <name>   creates a waystone record 12 blocks east of
//                                                      the player and activates it for them
//   /waystones inventorybutton debug cooldowns         toggles the test requirements (only the
//                                                      300 s inventory button cooldown)

#include <memory>

#include "ll/api/command/CommandHandle.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"

#include "feature/inventorybutton/CooldownRequirements.h"
#include "game/Waystones.h"
#include "game/WorldDb.h"
#include "mod/Commands.h"
#include "mod/Log.h"

namespace bs::feature::inventorybutton {

struct InventoryButtonDebugNameParams {
    std::string name;
};

namespace {
// Raw pointer on purpose: never restored during static destruction (the seam slot may be gone).
// Debug only: restoring puts back whatever was installed when it was turned on.
SeamSwap<service::IRequirements>* gTestRequirements = nullptr;

void testRequirementsOff() {
    delete gTestRequirements;
    gTestRequirements = nullptr;
}

struct WorldClosedRegistrar {
    WorldClosedRegistrar() {
        game::db::onWorldClosed([] { testRequirementsOff(); });
    }
} gWorldClosedRegistrar;
} // namespace

BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload<InventoryButtonDebugNameParams>()
        .text("inventorybutton")
        .text("debug")
        .text("waystone")
        .required("name")
        .execute([](CommandOrigin const& origin, CommandOutput& output, InventoryButtonDebugNameParams const& params) {
            if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
            auto* player = commands::playerOf(origin);
            if (!player) return output.error("Only players can use this.");
            auto                 feet = player->getFeetBlockPos();
            core::WaystoneRecord r;
            r.name    = params.name;
            r.pos     = {static_cast<int>(player->getDimensionId()), feet.x + 12, feet.y, feet.z};
            r.style   = "waystone";
            r.origin  = core::Origin::Player;
            r.facing  = 3; // east: arrivals land at x + 1
            auto& ws  = game::Waystones::get();
            auto* rec = ws.create(r);
            if (!rec) return output.error("No world.");
            ws.activate(*player, rec->uid);
            log().info(
                "inventory button debug: waystone {} at {} {} {} activated for {}",
                rec->name,
                r.pos.x,
                r.pos.y,
                r.pos.z,
                player->getRealName()
            );
            output.success("waystone " + rec->name + " created and activated");
        });
    cmd.overload()
        .text("inventorybutton")
        .text("debug")
        .text("cooldowns")
        .execute([](CommandOrigin const& origin, CommandOutput& output) {
            if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
            if (gTestRequirements) {
                testRequirementsOff();
                log().info("inventory button debug: test requirements off");
                output.success("test requirements off");
            } else {
                gTestRequirements = new SeamSwap<service::IRequirements>(std::make_unique<CooldownRequirements>());
                log().info("inventory button debug: test requirements on");
                output.success("test requirements on (inventory button cooldown 300 s)");
            }
        });
});

} // namespace bs::feature::inventorybutton
