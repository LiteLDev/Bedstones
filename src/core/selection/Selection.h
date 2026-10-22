#pragma once

// Pure logic of the waystone selection screen (docs/SPEC.md §4.1): sort modes, ordering,
// search, distance / cooldown formatting, label colours, group filter buttons and the panel
// layout. Shared by the server (model) and the client screen (layout, filtering).

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Warp.h"
#include "core/Waystone.h"

namespace bs::core::selection {

// ---- sort modes ---------------------------------------------------------------------------------

/// Waystones' `WaystoneSortMode`, cycled Manual -> By Name -> By Distance.
enum class SortMode { Manual, Name, Distance };

[[nodiscard]] std::string_view toString(SortMode m);           ///< "manual" | "name" | "distance"
[[nodiscard]] std::optional<SortMode> sortModeFromString(std::string_view s);
[[nodiscard]] SortMode               nextSortMode(SortMode m); ///< Manual -> Name -> Distance -> Manual

/// Where the viewer stands (distances are measured from here).
struct Viewpoint {
    int    dim = 0;
    double x = 0, y = 0, z = 0;
};

/// Euclidean distance from `from` to the centre of block `pos` (ignores the dimension).
[[nodiscard]] double distanceTo(Viewpoint const& from, GlobalPos const& pos);

/// Return portals and memorials are always listed first (`WarpTarget::pinned` or their kind).
[[nodiscard]] bool isPinned(WarpTarget const& t);

/// Sorts `targets` in place (stable):
/// - pinned entries first (in their original order),
/// - Manual: by position in `sortingIndex`, unknown ids last in their original order,
/// - Name: case-insensitive effective name,
/// - Distance: same dimension as `from` first, nearest first; other dimensions (and players, which
///   have no fixed position) after, by dimension id then distance.
void sortTargets(std::vector<WarpTarget>& targets, SortMode mode, std::vector<std::string> const& sortingIndex, Viewpoint const& from);

// ---- search / text ------------------------------------------------------------------------------

/// ASCII-lowercased copy (other UTF-8 bytes are kept).
[[nodiscard]] std::string toLower(std::string_view s);
/// Case-insensitive substring match of the effective name (empty query matches everything).
[[nodiscard]] bool matchesSearch(std::string_view name, std::string_view query);

/// "123m", or "1.2km" when `meters >= 10000` or `longName`.
[[nodiscard]] std::string formatDistance(double meters, bool longName = false);
/// Remaining cooldown as "mm:ss" (seconds rounded up; minutes may exceed 59).
[[nodiscard]] std::string formatCooldown(double seconds);

// ---- rows ---------------------------------------------------------------------------------------

/// A row is clickable: not the source itself and affordable (creative players can always warp).
[[nodiscard]] bool isSelectable(WarpTarget const& t, bool creative);

/// The XP widget: which enchanting-table level sprite (1..3, 0 = none) and whether "+" follows.
struct LevelSprite {
    int  sprite = 0;
    bool plus   = false;
};
[[nodiscard]] LevelSprite levelSprite(int levels);

/// Minimal group info the screen needs (from `IGroups::groups`).
struct GroupInfo {
    std::string id;
    std::string name;
    std::string icon;
    std::string color; ///< ChatFormatting name
    bool        hidden = false;
};

/// The label colour of a row (ChatFormatting name): warp portals "light_purple", otherwise the
/// colour of the first group in `groups` (player's group order, hidden ones skipped) that the
/// entry belongs to, otherwise "white".
[[nodiscard]] std::string labelColor(WarpTarget const& t, std::vector<GroupInfo> const& groups);

/// Group filter buttons: non-hidden groups (in order) with at least one entry in `targets`,
/// at most `maxButtons`.
[[nodiscard]] std::vector<GroupInfo>
filterGroups(std::vector<GroupInfo> const& groups, std::vector<WarpTarget> const& targets, int maxButtons);

// ---- layout -------------------------------------------------------------------------------------

inline constexpr int kPanelWidth   = 270;
inline constexpr int kHeaderHeight = 64;
inline constexpr int kRowHeight    = 22;
inline constexpr int kRowWidth     = 220;
inline constexpr int kButtonWidth  = 216;
inline constexpr int kButtonHeight = 20;
inline constexpr int kMinHeight    = 200;

/// Panel height: max(200, header + rows) capped at `screenHeight - 40`.
[[nodiscard]] int panelHeight(int rowCount, int screenHeight);
/// Group buttons that fit in the left column: 5, plus one per extra 22 px of panel height.
[[nodiscard]] int maxGroupButtons(int panelHeight);

} // namespace bs::core::selection
