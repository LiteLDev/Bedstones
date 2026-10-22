#pragma once

// U11: the Manage Waystones screen ("manage") and the Personal Settings screen
// ("personal_settings"), docs/SPEC.md §4.2 / §4.3. Server side (both builds).
//
// Screen protocol (net/Screens.h), model -> client:
//   manage:            {title, source, hasManageGroups, rows: [{id, name, kind, hidden, canDelete,
//                       canHide, everyone, current}]}
//   personal_settings: {uid, name, alias, canEdit, hasManageGroups, groups: [{id, name, icon,
//                       color}], group, favorite, source}
// Actions <- client (every one re-validated against the player's current list):
//   manage:            back, manage_groups, personal_settings {id}, swap {from, to},
//                      move_top {id}, move_bottom {id}, delete {id}, set_hidden {id, hidden}
//   personal_settings: save {alias, group, favorite} (then back to manage), edit {+save fields},
//                      manage_groups {+save fields}, back

#include <optional>
#include <string>
#include <vector>

#include "core/Warp.h"
#include "core/manage/ManageRules.h"

class Player;

namespace bs::feature::manage {

inline constexpr char const* kManageScreen   = "manage";
inline constexpr char const* kPersonalScreen = "personal_settings";

/// One row of the manage list.
struct Entry {
    core::WarpTarget         target;
    core::manage::EntryFacts facts;
    bool                     hidden = false;
};

/// The rows of the manage screen opened from `source`, in manual order: the selection targets
/// minus return portals and memorials (plus hidden entries a selection list may leave out).
[[nodiscard]] std::vector<Entry> entries(Player& player, core::WarpSource const& source);
[[nodiscard]] core::manage::ActorFacts actorFacts(Player& player);

/// Opens / refreshes the Manage Waystones screen.
void openManage(Player& player, core::WarpSource const& source);
/// Opens Personal Settings for a listed entry. Returns false when `uid` is not in the list.
bool openPersonal(Player& player, std::string const& uid, core::WarpSource const& source);

/// Personal Settings save (alias, user group, favorite) after validation. Returns false when
/// `uid` is not a listed entry or `group` is not one of the player's user groups.
bool savePersonal(Player& player, std::string const& uid, core::WarpSource const& source, std::string const& alias, std::string const& group, bool favorite);

/// The player stands within 4 blocks of the waystone (Edit Waystone button).
[[nodiscard]] bool withinEditRange(Player& player, core::GlobalPos const& pos);
/// Whether the block of `pos` still exists (nullopt: the chunk is not loaded).
[[nodiscard]] std::optional<bool> blockExists(core::GlobalPos const& pos);

} // namespace bs::feature::manage
