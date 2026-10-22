#pragma once

// Permissions, visibility and the global / team indexes (U9, docs/SPEC.md §3.3-3.4).
//
// The IPermissions seam is replaced by the Waystones rules while this unit is installed. The read
// helpers below give other units fast access to the indexes (server thread only; the returned
// pointers are valid for the current tick).
//
// Teams: Bedrock has no scoreboard teams. A player's teams are its `team:<name>` tags
// (`/tag @s add team:red`); two players are in the same team when they share such a tag. The tags
// of every player seen are cached per world (`bs:access:teams`) so offline owners keep their team.

#include <string>
#include <vector>

#include "core/Waystone.h"

class Player;

namespace bs::feature::access {

/// Tag prefix that puts a player in a team.
inline constexpr std::string_view kTeamTagPrefix = "team:";

/// Generated (wilderness / village / dungeon) and never named: hidden from global / team lists.
[[nodiscard]] bool isGeneratedUnnamed(core::WaystoneRecord const& record);

/// `restrictedWaystones` contains `origin`.
[[nodiscard]] bool isRestricted(core::Origin origin);
/// `defaultVisibility` from config (ACTIVATION when the value is not a valid waystone visibility).
[[nodiscard]] core::Visibility configuredDefaultVisibility();

/// The team names of an online player (its `team:<name>` tags); refreshes the team cache.
[[nodiscard]] std::vector<std::string> teamsOf(Player const& player);
/// The team names of a player by UUID string: live tags when online, the cached ones otherwise.
[[nodiscard]] std::vector<std::string> teamsOf(std::string const& playerUuid);
/// `viewer` shares at least one team with the player `otherUuid`.
[[nodiscard]] bool sameTeam(Player const& viewer, std::string const& otherUuid);

/// Waystones listed for everyone: GLOBAL, not transient, not generated-but-unnamed (index).
[[nodiscard]] std::vector<core::WaystoneRecord*> globalWaystones();
/// TEAM waystones (not transient, not generated-but-unnamed) whose owner shares a team with `viewer`.
/// The viewer's own TEAM waystones are included.
[[nodiscard]] std::vector<core::WaystoneRecord*> teamWaystones(Player const& viewer);
/// Rebuilds both indexes from the registry (done on world ready; exposed for tests).
void rebuildIndexes();

} // namespace bs::feature::access
