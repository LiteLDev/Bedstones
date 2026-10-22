#pragma once

// U7: the Waystones teleport pipeline (docs/SPEC.md §3.5), installed as the `ITeleport` seam.
//
// warp(): build the context (pets, leashed mobs, requirements) -> validate (alive, source range,
// warp item, leash rules, affordable) -> `service::intercept()` -> load the destination chunks
// (asynchronously when they are not loaded) -> validate again (target still there, block is a
// teleport target) -> consume requirements -> teleport the player, its mount / passengers, pets
// and leashed mobs -> effects, `IArrival::onArrived`, provider `onWarped`, `notifyWarped`.
//
// Open screens whose server context carries `context["source"]` (a `core::WarpSource` json with a
// block source) are closed when the player moves out of interaction range of that block
// (Java's `stillValid`, `canInteractWithBlock(pos, 4)`).

#include <optional>
#include <string>
#include <vector>

#include "ll/api/coro/CoroTask.h"

#include "core/Warp.h"
#include "core/teleport/TeleportRules.h"
#include "service/Services.h"

class Actor;
class Player;
class BlockSource;

namespace bs::feature::teleport {

/// Config section `teleports` (config/teleports.json). The requirement keys of Waystones'
/// `teleports` section (enableCosts, warpRequirements, ...) live in U8's `requirements` section.
struct TeleportsConfig {
    int                      version          = 1;
    std::string              transportPets    = "DISABLED"; ///< ENABLED / SAME_DIMENSION / DISABLED
    std::string              transportLeashed = "ENABLED";  ///< ENABLED / SAME_DIMENSION / DISABLED
    std::vector<std::string> entityDenyList{"minecraft:wither"};
};

[[nodiscard]] core::teleport::TransportMode transportPets();
[[nodiscard]] core::teleport::TransportMode transportLeashed();

/// The block source of dimension `dim` (created if needed), nullptr without a level.
[[nodiscard]] BlockSource* regionOf(int dim);
/// Loads the chunks of `pos` and its 4 neighbouring blocks (Waystones loads them before a warp);
/// true once they are loaded, false after ~5 s.
ll::coro::CoroTask<bool> preloadDestination(core::GlobalPos pos);
/// Pets that would come along (tamed, owned by `player`, within 10 blocks, not sitting, not
/// leashed, not denied), before the `transportPets` rule.
[[nodiscard]] std::vector<Actor*> findPets(Player& player);
/// Mobs leashed to `player` within 10 blocks.
[[nodiscard]] std::vector<Actor*> findLeashed(Player& player);
/// The source block of `source` is in range (true when the source has no block).
[[nodiscard]] bool sourceInRange(Player& player, core::WarpSource const& source);

/// Log / feedback name of a warp target.
[[nodiscard]] std::string targetName(std::string const& targetId);

} // namespace bs::feature::teleport
