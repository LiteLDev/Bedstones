#pragma once

// U4: Warp Stone, Warp / Return / Blank / Bound Scrolls (docs/SPEC.md §2, §3.5-3.7).
//
// - Charging: right-click with a chargeable item starts a charge (bow animation and use time come
//   from the item JSON's `use_modifiers` / `use_animation`; the server counts the configured
//   `items.warpStoneUseTime` / `items.scrollUseTime` ticks). Releasing, switching the item or
//   taking damage cancels it. A completed charge opens the selection (warp scroll / warp stone) or
//   warps directly (bound / return scroll).
// - Consumption happens in a warp listener, only after a successful warp, so it works with any
//   ITeleport implementation.
// - Blank scrolls used on a waystone-like block (anything with a record) become bound scrolls.

#include <optional>
#include <string>

#include "core/Waystone.h"
#include "core/scrolls/Scrolls.h"

class Player;

namespace bs::feature::scrolls {

/// Config section `items` (Waystones' warpStoneUseTime / scrollUseTime / enableDurability).
struct ItemsConfig {
    int  version          = 1;
    int  warpStoneUseTime = 32; ///< ticks to hold right-click with a warp stone
    int  scrollUseTime    = 32; ///< ticks to hold right-click with a scroll
    bool enableDurability = true; ///< warp stones lose 1 durability per successful warp
};

/// Starts charging the item in `hand` (0 main, 1 offhand). False if it is not chargeable or the
/// player is already charging. A use time of 0 completes immediately.
bool startCharge(Player& player, int hand = 0);
/// The player is charging a scroll / warp stone.
[[nodiscard]] bool isCharging(Player const& player);
/// Cancels a running charge (logs `scroll charge cancelled: <item> by <player> (<reason>)`).
void cancelCharge(Player& player, std::string const& reason);
/// What happens when a charge completes, for the item currently in `hand`.
void completeCharge(Player& player, core::scrolls::ScrollItem item, int hand = 0);

/// The Return Scroll's target for `player` ("" when there is none).
[[nodiscard]] std::string returnTargetFor(Player& player);

/// Binds the blank scroll in `hand` to the record `uid` (consumes one, gives a bound scroll).
bool bindBlankScroll(Player& player, std::string const& uid, int hand = 0);
/// The record of the waystone-like block at `pos` (its lower half also for upper-half clicks).
[[nodiscard]] core::WaystoneRecord* recordAtBlock(Player& player, core::GlobalPos const& pos);

} // namespace bs::feature::scrolls
