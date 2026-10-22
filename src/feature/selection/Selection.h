#pragma once

// U10: the waystone selection menu (docs/SPEC.md §3.7, §4.1), server side.
//
// - `ITargets`: the target set of every warp source (activated / global / team waystones,
//   sharestone and portstone colours, shards in the source's container, target providers).
// - `ISelection`: the "selection" screen (and "admin_selection" for `/waystones gui`), its model,
//   actions (select / sort / manage / edit_current / close) and the form fallback.
//
// Screen model ("selection" and "admin_selection"):
//   title, admin, hasSource, currentName, sortMode, showSort, showManage, canManage, emptyText,
//   groups: [{id, name, icon, color, hidden}],
//   entries: [{id, kind, name, dim, distance (-1 = other dimension), current, selectable, pinned,
//              global, color, groups, req: {xp, levels, levelDisplay, canAfford, cooldown,
//              items: [{id, count, missing}], refuse}}]
// Context: {source: WarpSource json, admin: bool, owner: uuid}.

#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "core/Warp.h"

class Player;

namespace bs::feature::selection {

inline constexpr char const* kScreen      = "selection";
inline constexpr char const* kAdminScreen = "admin_selection";

/// The target set for `source` (ITargets::list).
[[nodiscard]] std::vector<core::WarpTarget> computeTargets(Player& player, core::WarpSource const& source);
/// `/waystones gui`: the waystones `ownerUuid` activated, as seen by `viewer` (free).
[[nodiscard]] std::vector<core::WarpTarget> adminTargets(Player& viewer, std::string const& ownerUuid);

/// The screen model (sorted entries) for `player`.
[[nodiscard]] nlohmann::json buildModel(Player& player, core::WarpSource const& source, bool admin, std::string const& ownerUuid);

/// Opens the selection screen (ISelection::open).
void openSelection(Player& player, core::WarpSource const& source);
/// Opens the admin selection screen (ISelection::openAdmin).
void openAdminSelection(Player& viewer, std::string const& ownerUuid);

/// Display name of a player uuid (online player, LeviLamina's player cache, then the uuid).
[[nodiscard]] std::string playerName(std::string const& uuid);

// ---- player state ------------------------------------------------------------------------------------
[[nodiscard]] int   playerLevel(Player& player);
[[nodiscard]] float playerProgress(Player& player); ///< 0..1 into the current level
/// "ender_pearl" -> "minecraft:ender_pearl".
[[nodiscard]] std::string fullItemId(std::string const& id);
/// How many of item `id` the player carries (inventory + offhand).
[[nodiscard]] int countItem(Player& player, std::string const& id);

// ---- requirement preview (debug command / selftests) -----------------------------------------------
/// Wraps the active IRequirements so that records carrying `extra["selectionPreview"]` (a
/// Requirement json) get those requirements added. Lets the screen be shown with every widget
/// kind without the requirements unit. Off restores the wrapped implementation.
void setRequirementPreview(bool on);
[[nodiscard]] bool requirementPreview();

} // namespace bs::feature::selection
