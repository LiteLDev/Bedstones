#pragma once

// Waystone groups (docs/SPEC.md §4.4): the inbuilt groups, the player's user groups and the pure
// operations behind the Manage Groups / Edit Group screens. No game headers; the feature unit
// (feature/groups) wires this to player data, screens and the IGroups seam.
//
// Storage: a player's `PlayerWaystoneData::groupRegistry` holds every user group
// ("waystones:user/<uuid>", inbuilt = false) and, for inbuilt groups, only the player's overrides
// (inbuilt = true; an empty name means "the default, translated" name). `resolve` merges both.

#include <array>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Waystone.h"

namespace bs::core::groups {

inline constexpr std::string_view kFavorites       = "waystones:favorites";
inline constexpr std::string_view kPlayers         = "waystones:players";
inline constexpr std::string_view kGlobal          = "waystones:global";
inline constexpr std::string_view kTeams           = "waystones:teams";
inline constexpr std::string_view kDimensionPrefix = "waystones:dimension/";
inline constexpr std::string_view kUserPrefix      = "waystones:user/";

/// Preset icons in cycle order (Edit Group icon button): Waystones' presets, then the dimension icons.
inline constexpr std::array<std::string_view, 13> kIcons{
    "community_hubs", "dimension", "dungeons",  "favorites",  "global",     "player_homes", "players",
    "resource_sites", "teams",     "villages",  "overworld",  "the_nether", "the_end",
};
/// Label colours in cycle order (ChatFormatting names).
inline constexpr std::array<std::string_view, 15> kColors{
    "white",     "gold",        "light_purple", "aqua", "yellow",    "green",      "red",      "gray",
    "dark_gray", "dark_aqua",   "dark_purple",  "blue", "dark_blue", "dark_green", "dark_red",
};
/// Longest group name accepted (code points).
inline constexpr std::size_t kMaxNameLength = 128;
/// Most user groups a player can have (keeps player data bounded against a misbehaving client).
inline constexpr std::size_t kMaxUserGroups = 100;

[[nodiscard]] std::string dimensionGroup(int dim);
[[nodiscard]] bool        isUserGroup(std::string_view id);
/// One of the inbuilt ids (favorites, players, global, teams, the three dimensions).
[[nodiscard]] bool        isInbuilt(std::string_view id);
[[nodiscard]] bool        isIcon(std::string_view icon);
[[nodiscard]] bool        isColor(std::string_view color);

/// The inbuilt groups with their default name (English), icon, colour and sort index, in order.
[[nodiscard]] std::vector<WaystoneGroup> inbuiltDefaults();
/// Translation key of an inbuilt group's default name ("waystones.groups.favorites", ...).
[[nodiscard]] std::string nameKey(std::string_view id);
/// Translation key of an icon / colour name ("gui.waystones.group_settings.icon.<id>", "color.waystones.group.<id>").
[[nodiscard]] std::string iconKey(std::string_view icon);
[[nodiscard]] std::string colorKey(std::string_view color);
/// English names of icons / colours ("community_hubs" -> "Community Hubs").
[[nodiscard]] std::string displayName(std::string_view id);

/// Translates an inbuilt default name: (translation key, English fallback) -> text.
using Translator = std::function<std::string(std::string const& key, std::string const& fallback)>;

/// Every group of a player: inbuilt (with the player's overrides) + user groups, sorted by
/// sortIndex, then (untranslated) name.
[[nodiscard]] std::vector<WaystoneGroup> resolve(std::vector<WaystoneGroup> const& registry, Translator const& tr = {});
/// One group of `resolve`, nullopt when unknown.
[[nodiscard]] std::optional<WaystoneGroup> find(std::vector<WaystoneGroup> const& registry, std::string_view id, Translator const& tr = {});

/// Trims, drops control characters and § codes, and caps a name to kMaxNameLength code points.
[[nodiscard]] std::string sanitizeName(std::string_view name);
/// The next / previous entry of a cycle list (unknown -> the first).
[[nodiscard]] std::string cycleIcon(std::string_view icon, int direction);
[[nodiscard]] std::string cycleColor(std::string_view color, int direction);

/// Creates a user group with id "waystones:user/<uuid>", sorted last. Invalid icon / colour fall
/// back to the first preset. Returns the id ("" when the player already has kMaxUserGroups).
std::string create(std::vector<WaystoneGroup>& registry, std::string_view name, std::string_view icon, std::string_view color, std::string const& uuid);
/// Updates name / icon / colour of any group (inbuilt groups store an override; a name equal to
/// `defaultName` or empty keeps the translated default). False: unknown id or invalid icon / colour.
bool update(
    std::vector<WaystoneGroup>& registry,
    std::string_view            id,
    std::string_view            name,
    std::string_view            icon,
    std::string_view            color,
    std::string_view            defaultName = {}
);
/// Deletes a user group and drops it from the per-waystone assignments. False for inbuilt / unknown ids.
bool remove(std::vector<WaystoneGroup>& registry, std::map<std::string, std::vector<std::string>>& assignments, std::string_view id);
/// Hides / unhides an inbuilt group. False for user / unknown ids.
bool setHidden(std::vector<WaystoneGroup>& registry, std::string_view id, bool hidden);
/// Swaps the positions of two groups in the player's order (renumbers every sort index).
bool swap(std::vector<WaystoneGroup>& registry, std::string_view a, std::string_view b);
/// Moves a group to `index` of the order (clamped), shifting the others.
bool moveTo(std::vector<WaystoneGroup>& registry, std::string_view id, int index);

/// Implicit groups of a list entry: global, players, teams, its dimension.
[[nodiscard]] std::vector<std::string> implicitGroups(std::string_view kind, bool global, bool team, int dim);
/// The explicit assignment of a waystone, minus groups that no longer exist; then the implicit ones.
[[nodiscard]] std::vector<std::string>
groupsOf(std::vector<WaystoneGroup> const& registry, std::vector<std::string> const& assigned, std::vector<std::string> const& implicit);

} // namespace bs::core::groups
