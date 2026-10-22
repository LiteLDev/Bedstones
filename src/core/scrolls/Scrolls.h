#pragma once

// Pure rules of the scrolls and the warp stone (docs/SPEC.md §2, §3.5-3.7): which item does what,
// the charge schedule, the Return Scroll target choice and the warp stone durability.

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Warp.h"
#include "core/Waystone.h"

namespace bs::core::scrolls {

enum class ScrollItem { WarpStone, WarpScroll, ReturnScroll, BlankScroll, BoundScroll };

/// The item behind a "bedstones:" item path ("warp_scroll", ...); nullopt for other items.
[[nodiscard]] std::optional<ScrollItem> fromPath(std::string_view path);
[[nodiscard]] std::string_view          path(ScrollItem item);
/// Items charged by holding right-click (everything but the blank scroll).
[[nodiscard]] bool chargeable(ScrollItem item);
/// The warp source kind a charged item starts (nullopt for the blank scroll).
[[nodiscard]] std::optional<WarpSourceKind> sourceKind(ScrollItem item);
/// Scrolls are consumed (-1) after a successful warp; the warp stone loses durability instead.
[[nodiscard]] bool consumedOnWarp(ScrollItem item);
/// Charge time in ticks from the `items` config.
[[nodiscard]] int useTime(ScrollItem item, int warpStoneUseTime, int scrollUseTime);

/// Particles spawned on one charge tick (Waystones' client `onUseTick`, every 5 remaining ticks).
struct ChargeParticles {
    int reversePortal = 0;
    int crimsonSpore  = 0;
    int witch         = 0;
    int portal        = 0; ///< the burst on the last tick
    [[nodiscard]] bool any() const { return reversePortal + crimsonSpore + witch + portal > 0; }
};
/// Particles for tick `elapsed` (1-based) of a `duration`-tick charge.
[[nodiscard]] ChargeParticles chargeParticles(ScrollItem item, int elapsed, int duration);

/// A Return Scroll candidate waystone.
struct Candidate {
    std::string uid;
    GlobalPos   pos;
};
/// Nearest activated waystone in `dim` to (x, y, z) (block centres, squared distance), "" if none.
[[nodiscard]] std::string nearest(std::vector<Candidate> const& activated, int dim, double x, double y, double z);
/// Return Scroll target: the return portal if it still exists, otherwise the nearest waystone.
[[nodiscard]] std::string returnTarget(std::string const& returnPortalUid, bool returnPortalExists, std::string const& nearestUid);

/// Blank scroll binding (Waystones' `BlankScrollItem.useOn`): bind when the bound scroll fits
/// somewhere, or when the blank stack in hand is about to be used up anyway (not in creative).
[[nodiscard]] bool canBind(bool inventoryHasRoom, bool creative, int handCount);

/// Lore line of a bound scroll ("Bound to: <name>", gray with the name in aqua).
[[nodiscard]] std::string boundLore(std::string_view boundToTemplate, std::string_view name);

} // namespace bs::core::scrolls
