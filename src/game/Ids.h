#pragma once

// Block / item identifiers and block state names (the behavior pack must use exactly these).
// Every id is "bedstones:<path>"; paths follow Waystones 21.1.46 (docs/SPEC.md §1-2).

#include <array>
#include <string>
#include <string_view>

#include "core/Waystone.h"

namespace bs::ids {

inline constexpr std::string_view kNamespace = "bedstones";

/// "bedstones:" + path.
[[nodiscard]] inline std::string id(std::string_view path) { return std::string(kNamespace) + ":" + std::string(path); }
/// Path of a "bedstones:" id ("" if it is not one of ours).
[[nodiscard]] inline std::string_view pathOf(std::string_view fullId) {
    if (fullId.size() <= kNamespace.size() + 1 || !fullId.starts_with(kNamespace) || fullId[kNamespace.size()] != ':') return {};
    return fullId.substr(kNamespace.size() + 1);
}

// ---- block states (integer / boolean custom states, declared in every block JSON) ------------
/// int 0..3, Java 2D data value of the facing: 0 south, 1 west, 2 north, 3 east.
inline constexpr std::string_view kStateFacing = "bedstones:facing";
/// int 0/1: the upper half of a two-tall block (all custom states are integers).
inline constexpr std::string_view kStateUpper = "bedstones:upper";
/// int: warp plate status (see `WarpPlateStatus`).
inline constexpr std::string_view kStateStatus = "bedstones:status";

// ---- waystones (two tall) -----------------------------------------------------------------------
/// Block paths of the 10 waystone styles (Waystones' ids). The default style is "waystone".
inline constexpr std::array<std::string_view, 10> kWaystoneStyles{
    "waystone",
    "mossy_waystone",
    "sandy_waystone",
    "deepslate_waystone",
    "blackstone_waystone",
    "end_stone_waystone",
    "red_nether_bricks_waystone",
    "purpur_waystone",
    "prismarine_waystone",
    "mud_bricks_waystone",
};
[[nodiscard]] inline bool isWaystoneStyle(std::string_view path) {
    for (auto s : kWaystoneStyles) {
        if (s == path) return true;
    }
    return false;
}

// ---- sharestones / portstones (two tall) -----------------------------------------------------
/// "<color>_sharestone" for the 15 non-white dye colors.
[[nodiscard]] inline std::string sharestonePath(std::string_view color) { return std::string(color) + "_sharestone"; }
/// "<color>_portstone" for all 16 dye colors ("white_portstone" is named "Portstone").
[[nodiscard]] inline std::string portstonePath(std::string_view color) { return std::string(color) + "_portstone"; }

// ---- other blocks --------------------------------------------------------------------------------
inline constexpr std::string_view kWarpPlate        = "warp_plate";
inline constexpr std::string_view kWarpPortal       = "warp_portal";       ///< two tall, no item
inline constexpr std::string_view kFleetingMemorial = "fleeting_memorial"; ///< no item

/// Warp plate `bedstones:status` values.
enum class WarpPlateStatus { Empty = 0, Idle, Attuning, Warping, WarpingInvalid, RedstoneDisabled, Locked };

// ---- items -----------------------------------------------------------------------------------------
inline constexpr std::string_view kWarpStone             = "warp_stone";
inline constexpr std::string_view kWarpScroll            = "warp_scroll";
inline constexpr std::string_view kReturnScroll          = "return_scroll";
inline constexpr std::string_view kBlankScroll           = "blank_scroll";
inline constexpr std::string_view kBoundScroll           = "bound_scroll";
inline constexpr std::string_view kPortalScroll          = "portal_scroll";
inline constexpr std::string_view kWarpDust              = "warp_dust";
inline constexpr std::string_view kDormantShard          = "dormant_shard";
inline constexpr std::string_view kAttunedShard          = "attuned_shard";
inline constexpr std::string_view kCrumblingAttunedShard = "crumbling_attuned_shard";
inline constexpr std::string_view kDeepslateShard        = "deepslate_shard";
inline constexpr std::string_view kTwinboundFeather      = "twinbound_feather";
inline constexpr std::string_view kEpitaph               = "epitaph";

// ---- item user-data keys (custom NBT strings) ------------------------------------------------
/// Waystone uid on a silk-touched waystone / warp plate item (Waystones' `waystone` component).
inline constexpr std::string_view kNbtWaystone = "bs_waystone";
/// Attunement target uid on attuned shards.
inline constexpr std::string_view kNbtAttunement = "bs_attunement";
/// Bound scroll target uid / name.
inline constexpr std::string_view kNbtBoundUid  = "bs_bound_uid";
inline constexpr std::string_view kNbtBoundName = "bs_bound_name";
/// Twinbound feather id and linked feather id.
inline constexpr std::string_view kNbtFeather     = "bs_feather";
inline constexpr std::string_view kNbtFeatherLink = "bs_feather_link";

} // namespace bs::ids
