#pragma once

// Portal Scroll + Warp Portal (docs/SPEC.md §1.6, §2, §3.7).
//
// - The Portal Scroll charges for 32 ticks, then opens the selection menu (source PortalScroll).
// - A warp interceptor takes PortalScroll warps over: instead of teleporting, it opens a two-tall
//   Warp Portal ahead of the player, backed by a transient `warp_portal` record named
//   "Return to Portal" whose `extra["portalTarget"]` is the chosen target id.
// - Entities entering the portal warp to that target; players remember the portal in
//   `playerData.extra["returnPortal"]` (read by the Return Scroll) and see it pinned as
//   "Return to Portal" in selection menus.

#include <optional>
#include <string>

#include "core/Waystone.h"
#include "core/portal/Placement.h"

class Player;

namespace bs::feature::portal {

/// Record `extra` keys.
inline constexpr char const* kExtraTarget    = "portalTarget";
inline constexpr char const* kExtraTicksLeft = "portalTicksLeft";
/// Player data `extra` key (shared contract with the Return Scroll).
inline constexpr char const* kExtraReturnPortal = "returnPortal";

/// Where a portal would open in front of `player` (nullopt: no space).
[[nodiscard]] std::optional<core::portal::Cell> portalSpotFor(Player& player);

/// Places the portal blocks at `lower` and creates its record. Returns the record uid ("" on failure).
std::string spawnPortal(int dim, core::portal::Cell lower, int facing, std::string const& targetId, Player const* owner);
/// Removes a portal (blocks + record). `expired` plays the closing effects.
void removePortal(std::string const& uid, bool expired);

/// The Portal Scroll finished charging in `hand`: checks for space, then opens the selection menu.
/// Returns false when the portal has no space (error shown in the action bar).
bool onScrollCharged(Player& player, int hand);

// ---- test hooks ----------------------------------------------------------------------------------
/// Sets the remaining lifetime of a portal (accelerates expiry in selftests).
void setTicksLeft(std::string const& uid, int ticks);
/// Clears the per-entity re-use cooldowns.
void clearCooldowns();
/// Number of completed Portal Scroll charges since the mod was enabled.
[[nodiscard]] int chargeCount();

} // namespace bs::feature::portal
