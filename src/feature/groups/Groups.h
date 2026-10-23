#pragma once

// U12 Groups: the IGroups seam (inbuilt + user groups, hidden state, order, groupsOf) and the
// Manage Groups / Edit Group screens (docs/SPEC.md §4.4). Server thread only.

#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "core/Waystone.h"
#include "core/groups/GroupRegistry.h"

class Player;

namespace bs::feature::groups {

inline constexpr char const* kManageScreen = "manage_groups";
inline constexpr char const* kEditScreen   = "edit_group";

/// Translates inbuilt group names in the player's locale.
[[nodiscard]] core::groups::Translator translatorFor(Player const& player);
/// The player's groups (inbuilt + user), sorted, names translated ("Unnamed Group" for empty names).
[[nodiscard]] std::vector<core::WaystoneGroup> groupsFor(Player& player);

/// Opens Manage Groups. `args`: {"source": WarpSource json, "from": "<screen>", "uid": "..."}.
void openManage(Player& player, nlohmann::json const& args);
/// Opens Edit Group for `groupId` ("" = create a new group). `returnArgs` reopen Manage Groups.
void openEdit(Player& player, std::string const& groupId, nlohmann::json const& returnArgs);

} // namespace bs::feature::groups
