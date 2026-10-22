#pragma once

// Inventory button ("Return to Waystone", docs/SPEC.md §3.8), server side.
//
// The config section `inventoryButton` lives on the server. Every companion player gets the
// button state on `inventory_button.state` (on join, when it changes and every second while a
// cooldown counts down); the client mod draws the button over the vanilla inventory and sends
// `inventory_button.warp` when it is used. Vanilla clients use `/waystones inventorybutton`.

#include <string>

#include "nlohmann/json.hpp"

#include "core/inventorybutton/InventoryButton.h"

class Player;

namespace bs::feature::inventorybutton {

/// `config/inventoryButton.json` (Waystones' `inventoryButton` section).
struct InventoryButtonConfig {
    int version = 1;
    /// "" / "NONE" (off), "NEAREST", "ANY" or the name of a waystone.
    std::string inventoryButton;
    int         inventoryButtonX         = 58;
    int         inventoryButtonY         = 60;
    int         creativeInventoryButtonX = 88;
    int         creativeInventoryButtonY = 33;
};

using core::inventorybutton::kStateChannel;
using core::inventorybutton::kWarpChannel;
/// Waystones' cooldown id of the button.
inline constexpr char const* kCooldownId = "waystones:inventory_button";

/// The button state of `player` under the current config (target, requirement, affordability).
[[nodiscard]] core::inventorybutton::ButtonState computeState(Player& player);

/// Uses the button for `player` (the `inventory_button.warp` handler; re-validates everything).
/// `expectedTarget`: the target the player confirmed (client confirm screen); when it no longer
/// matches the server's choice the warp is refused. Returns true when a warp / the selection
/// menu was started.
bool use(Player& player, std::string const& expectedTarget = {});

/// Sends the state to `player` now if it differs from the last one sent (`force`: always).
void syncState(Player& player, bool force = false);

} // namespace bs::feature::inventorybutton
