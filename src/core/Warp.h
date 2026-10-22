#pragma once

// Pure warp descriptions shared by the server and the client UI (docs/SPEC.md §3.5-3.7).

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "nlohmann/json.hpp"

#include "core/Waystone.h"

namespace bs::core {

/// What started a warp (Waystones' `WarpMode` / source flags).
enum class WarpSourceKind {
    Waystone,
    Sharestone,
    Portstone,
    WarpPlate,
    InventoryButton,
    WarpScroll,
    BoundScroll,
    ReturnScroll,
    PortalScroll,
    WarpStone,
    WarpPortal,
    Admin,   ///< `/waystones gui`
    Command, ///< other commands / API
};
[[nodiscard]] std::string_view                toString(WarpSourceKind k);
[[nodiscard]] std::optional<WarpSourceKind>   warpSourceFromString(std::string_view s);

struct WarpSource {
    WarpSourceKind kind = WarpSourceKind::Command;
    std::string    fromUid;   ///< the waystone / sharestone / plate the player stands at ("" = none)
    GlobalPos      fromPos;   ///< position of the source block (valid when it is a block)
    std::string    itemId;    ///< warp item id (scrolls, warp stone)
    std::string    portstoneColor; ///< portstone sources: its dye color ("white" lists waystones)
    int            hand = 0;  ///< 0 main hand, 1 offhand (item sources)

    [[nodiscard]] bool               isBlock() const;
    [[nodiscard]] bool               isItem() const;
    [[nodiscard]] nlohmann::json     toJson() const;
    [[nodiscard]] static WarpSource  fromJson(nlohmann::json const& j);
};

/// One cost line (rendered by the selection screen and tooltips).
struct ItemCost {
    std::string id;
    int         count = 0;
};
struct CooldownCost {
    std::string id;           ///< "waystones:inventory_button"
    double      seconds = 0;  ///< cooldown to apply after the warp
    bool        soft    = false;
};

/// The resolved requirements of one warp (Waystones' `WarpRequirement` aggregate).
struct Requirement {
    int                       xpPoints = 0; ///< experience points to pay
    int                       levels   = 0; ///< whole levels to pay
    std::vector<ItemCost>     items;
    std::vector<CooldownCost> cooldowns;
    /// Remaining active cooldown (seconds) that blocks the warp; 0 = none.
    double                    activeCooldown = 0;
    std::string               refuse;   ///< non-empty: always refused with this message / "$key"
    bool                      dismount = false;

    [[nodiscard]] bool               free() const;
    [[nodiscard]] nlohmann::json     toJson() const;
    [[nodiscard]] static Requirement fromJson(nlohmann::json const& j);
};

/// One entry of a selection list (any target kind).
struct WarpTarget {
    std::string id;     ///< waystone uid, or provider-specific ("player:<uuid>", ...)
    std::string kind;   ///< "waystone", "sharestone", "warp_plate", "warp_portal", "fleeting_memorial", "player", ...
    std::string name;   ///< effective (personalized) name
    GlobalPos   pos;
    bool        global   = false;
    bool        current  = false; ///< the source waystone itself (shown disabled)
    bool        pinned   = false; ///< return portals / memorials: always listed first
    bool        canAfford = true;
    Requirement requirement;
    std::vector<std::string> groups; ///< group ids
    nlohmann::json extra = nlohmann::json::object();

    [[nodiscard]] nlohmann::json    toJson() const;
    [[nodiscard]] static WarpTarget fromJson(nlohmann::json const& j);
};

/// Minecraft XP maths (Java `Player.getXpNeededForNextLevel` and total-points formulas).
[[nodiscard]] int xpForLevel(int level);          ///< points needed to go from `level` to `level+1`
[[nodiscard]] int totalXpForLevel(int level);     ///< points needed to reach `level` from 0
/// Levels a cost of `points` corresponds to for someone at `level` with `progress` (0..1) into it.
[[nodiscard]] int levelsForPoints(int level, float progress, int points);

} // namespace bs::core
