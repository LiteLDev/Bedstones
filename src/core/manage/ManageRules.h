#pragma once

// Pure rules of the Manage Waystones and Personal Settings screens (docs/SPEC.md §4.2, §4.3):
// manual ordering on the per-player sorting index, who may delete / hide which entry and what a
// delete does, alias normalisation and the group cycle. No game headers.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace bs::core::manage {

// ---- ordering --------------------------------------------------------------------------------------

/// `ids` in manual order: the ones in `sortingIndex` first (in index order), then the rest in
/// their given order.
[[nodiscard]] std::vector<std::string> ordered(std::vector<std::string> const& ids, std::vector<std::string> const& sortingIndex);

/// Makes sure every id of `displayed` (already in manual order) is in `sortingIndex`; missing
/// ones are appended in display order, which keeps the displayed order unchanged.
void ensureIndexed(std::vector<std::string>& sortingIndex, std::vector<std::string> const& displayed);

/// Swaps two listed entries (one drag step). Both must be in `displayed`. Returns false (index
/// unchanged) otherwise or when `a == b`.
bool swap(std::vector<std::string>& sortingIndex, std::vector<std::string> const& displayed, std::string const& a, std::string const& b);

/// Moves a listed entry to the top / bottom of the manual order (double-click /
/// shift-double-click). Returns false when `id` is not listed or already there.
bool moveToTop(std::vector<std::string>& sortingIndex, std::vector<std::string> const& displayed, std::string const& id);
bool moveToBottom(std::vector<std::string>& sortingIndex, std::vector<std::string> const& displayed, std::string const& id);

// ---- delete / hide ------------------------------------------------------------------------------------

/// What the manage screen knows about one entry.
struct EntryFacts {
    bool waystone   = false; ///< record type "waystone"
    bool sharestone = false;
    bool global     = false;
    bool team       = false;
    bool activated  = false; ///< activated by the viewing player
};

/// What it knows about the player.
struct ActorFacts {
    bool creative        = false;
    bool canManageGlobal = false; ///< IPermissions::canManageGlobal (creative or config)
};

enum class DeleteAction {
    None,   ///< not allowed
    Forget, ///< deactivate for this player
    Purge,  ///< remove the record from the world (creative + sharestone)
    Demote, ///< global -> activation (purged when its block is gone) and forgotten by this player
};

/// Delete rules: waystone-type entries; global only with global management; team only when
/// activated; sharestones only in creative.
[[nodiscard]] bool         canDelete(EntryFacts const& e, ActorFacts const& a);
[[nodiscard]] DeleteAction deleteAction(EntryFacts const& e, ActorFacts const& a);
/// The eye toggle replaces the delete X for global, team or sharestone entries the player can't delete.
[[nodiscard]] bool canHide(EntryFacts const& e, ActorFacts const& a);
/// The delete tooltip adds "This will remove the waystone for everyone." (global and sharestones).
[[nodiscard]] bool deleteAffectsEveryone(EntryFacts const& e);

// ---- personal settings -----------------------------------------------------------------------------

/// Maximum alias length in code points.
inline constexpr std::size_t kMaxAliasLength = 128;

/// The alias to store: control characters dropped, clipped to 128 code points; nullopt (= remove
/// the alias) when blank.
[[nodiscard]] std::optional<std::string> normalizeAlias(std::string const& alias);

/// Next (`dir` > 0) or previous (`dir` < 0) user group of the cycle "" (no group) -> g0 -> g1 ...
/// -> "" over `userGroups`. An unknown `current` counts as "".
[[nodiscard]] std::string cycleGroup(std::vector<std::string> const& userGroups, std::string const& current, int dir);

/// The user group of an entry: the first of `assigned` that is in `userGroups` ("" if none).
[[nodiscard]] std::string userGroupOf(std::vector<std::string> const& assigned, std::vector<std::string> const& userGroups);

/// Replaces the user-group membership in `assigned` with `group` ("" = none), keeping inbuilt
/// memberships (favorites). Ids starting with "waystones:user/" count as user groups too.
void setUserGroup(std::vector<std::string>& assigned, std::vector<std::string> const& userGroups, std::string const& group);

/// Adds / removes `groupId` (e.g. favorites). Returns true when the list changed.
bool setMember(std::vector<std::string>& assigned, std::string const& groupId, bool member);

} // namespace bs::core::manage
