#pragma once

// Inventory button ("Return to Waystone", docs/SPEC.md §3.8): pure logic shared by the server
// (state computation, re-validation) and the client mod (rendering the synced state).

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "nlohmann/json.hpp"

#include "core/Warp.h"
#include "core/Waystone.h"

namespace bs::core::inventorybutton {

/// Message channels (server -> client state, client -> server use).
inline constexpr char const* kStateChannel = "inventory_button.state";
inline constexpr char const* kWarpChannel  = "inventory_button.warp";

/// The `inventoryButton` config value, parsed.
enum class Mode {
    None,    ///< "" or "NONE": no button (default)
    Nearest, ///< "NEAREST": the nearest activated waystone in the dimension (return portal first)
    Any,     ///< "ANY": opens the selection menu
    Named,   ///< any other value: the activated waystone with that name
};

/// `config` without surrounding whitespace (the value modes and names are matched with).
[[nodiscard]] std::string_view trim(std::string_view config);
[[nodiscard]] Mode             parseMode(std::string_view config);
[[nodiscard]] std::string_view toString(Mode m); ///< "none" | "nearest" | "any" | "named"
[[nodiscard]] Mode             modeFromString(std::string_view s);

/// A waystone the button may target.
struct Candidate {
    std::string uid;
    std::string name;  ///< record name (matched by named mode)
    std::string alias; ///< personalized name ("" = none)
    GlobalPos   pos;
};

/// Index of the candidate nearest to (x, y, z) in dimension `dim` (block centres compared), or
/// nullopt when none is in that dimension. Ties keep the earlier candidate (activation order).
[[nodiscard]] std::optional<std::size_t>
nearest(std::vector<Candidate> const& candidates, int dim, double x, double y, double z);

/// Index of the candidate whose name (or, failing that, alias) equals `name` exactly.
[[nodiscard]] std::optional<std::size_t> byName(std::vector<Candidate> const& candidates, std::string_view name);

/// What the client needs to draw the button (sent on `inventory_button.state`).
struct ButtonState {
    Mode        mode      = Mode::None;
    bool        hasTarget = false; ///< nearest / named: a target exists; any: the player activated a waystone
    std::string targetId;          ///< waystone uid (nearest / named)
    std::string targetName;        ///< effective name of the target
    bool        canAfford       = true;
    int         cooldownSeconds = 0; ///< remaining blocking cooldown, rounded up
    Requirement requirement;
    int         x = 58, y = 60;                 ///< survival inventory offset
    int         creativeX = 88, creativeY = 33; ///< creative inventory offset

    [[nodiscard]] nlohmann::json     toJson() const;
    [[nodiscard]] static ButtonState fromJson(nlohmann::json const& j);
};

/// Whole seconds of a blocking cooldown shown in the tooltip ("Cooldown: N seconds").
[[nodiscard]] int cooldownSeconds(double activeCooldown);

} // namespace bs::core::inventorybutton
