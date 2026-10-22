#pragma once

// Edit Waystone screen (U13, docs/SPEC.md §3.4 save rules, §4.5): the IEditScreen seam, the
// "edit" screen opener, its actions, form fallback and the server-side save validation.

#include <string>

#include "nlohmann/json.hpp"

class Player;

namespace bs::feature::edit {

enum class SaveResult {
    Saved,
    NotFound, ///< the record is gone
    Denied,   ///< IPermissions::canEdit refused
    TooFar,   ///< more than 4 blocks from the block
};

/// Validates and applies a save of the edit screen (name + visibility) for `uid`.
SaveResult save(Player& player, std::string const& uid, std::string const& name, std::string const& visibility);

/// Opens the edit screen of `uid` (read-only when editing is denied). `source` is passed on to the
/// Personal Settings screen (empty = this waystone).
bool openEdit(Player& player, std::string const& uid, nlohmann::json const& source = nullptr);

/// The model the edit screen is opened with (also used by selftests).
[[nodiscard]] nlohmann::json buildModel(Player& player, std::string const& uid);

} // namespace bs::feature::edit
