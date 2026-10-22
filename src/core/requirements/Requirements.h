#pragma once

// Waystones' warp requirement language (docs/SPEC.md §3.6), pure C++.
//
// A rule list is a sequence of lines `[cond1, cond2(arg)] function(args)`, applied in order to an
// aggregate requirement (XP points, levels, items, cooldowns, dismount, refuse). Every condition
// has an automatic negated form (`source_is_not_warp_plate`, `not_has_cooldown`, ...). The game
// side fills a `Context` with what the conditions and variables need and turns the `Evaluated`
// aggregate into a `core::Requirement`.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Warp.h"

namespace bs::core::requirements {

/// The default `warpRequirements` of Waystones 21.1.46.
[[nodiscard]] std::vector<std::string> defaultRules();

/// One `name(args)` call (a condition or the function).
struct Call {
    std::string              name;
    std::vector<std::string> args; ///< raw, trimmed, unquoted
};

struct Rule {
    std::vector<Call> conditions;
    Call              function;
    std::string       source; ///< the original line (for messages)
};

/// Parses one line. Returns nullopt and sets `error` when the line is invalid (syntax, unknown
/// function / condition, wrong argument count or type).
[[nodiscard]] std::optional<Rule> parseRule(std::string_view line, std::string* error = nullptr);

struct Program {
    std::vector<Rule>        rules;
    std::vector<std::string> errors; ///< one message per skipped line
};
/// Parses every line; invalid ones are skipped and reported in `errors`. Empty lines are ignored.
[[nodiscard]] Program parse(std::vector<std::string> const& lines);

// ---- names -------------------------------------------------------------------------------------

/// The negated name of a condition: `not_` after the first `is_`, else a `not_` prefix.
[[nodiscard]] std::string negatedName(std::string_view condition);
/// Every known condition name (positive forms only).
[[nodiscard]] std::vector<std::string> conditionNames();
/// Every known function name.
[[nodiscard]] std::vector<std::string> functionNames();
/// Cooldown ids: bare names get the `waystones:` namespace.
[[nodiscard]] std::string cooldownId(std::string_view id);
/// Item ids: `waystones:x` becomes `bedstones:x`, bare names get `minecraft:`.
[[nodiscard]] std::string itemId(std::string_view id);
/// Entity type ids: bare names get `minecraft:`.
[[nodiscard]] std::string entityId(std::string_view id);
/// Dimension ids: `overworld`/`the_nether`/`the_end` (any namespace, also `nether`/`end`) or a
/// number. nullopt when unknown.
[[nodiscard]] std::optional<int> dimensionId(std::string_view id);

// ---- evaluation --------------------------------------------------------------------------------

/// Everything the conditions and variables look at, filled by the game side.
struct Context {
    // source
    WarpSourceKind sourceKind = WarpSourceKind::Command;
    std::string    sourceType;  ///< record type of the source block ("waystone", "red_sharestone", ...), "" if none
    std::string    sourceName;
    int            sourceDim = 0; ///< dimension of the source block, or of the player when there is none
    // target
    std::string targetType; ///< record type ("waystone", "warp_plate", "red_sharestone", ...) or "player"
    std::string targetName;
    int         targetDim    = 0;
    bool        targetGlobal = false;
    // context
    int    playerDim = 0;
    double distance  = 0; ///< blocks between the player and the target
    int    leashed   = 0;
    int    pets      = 0;
    int    passengers = 0;
    // player state
    bool                                onVehicle = false;
    std::string                         vehicleType;     ///< "minecraft:horse"
    std::vector<std::string>            vehicleFamilies; ///< Bedrock type families (`#tag` matches these)
    std::map<std::string, std::int64_t> cooldownRemainingMs; ///< active cooldowns (id -> ms left)
    std::map<std::string, int>          itemCounts;          ///< item id -> count over the inventory
    bool                                emptyInventory = true;
    bool                                wearingArmor   = false;
    std::vector<std::string>            tags;
};

struct Options {
    bool enableCosts     = true; ///< level / xp / item cost functions
    bool enableCooldowns = true; ///< cooldown and soft cooldown functions
};

/// The aggregate after applying every rule (Waystones' combined `WarpRequirement`).
struct Evaluated {
    int                                      xpPoints = 0;
    int                                      levels   = 0;
    std::vector<std::pair<std::string, int>> items;         ///< in first-use order
    struct Cooldown {
        std::string id;
        double      seconds = 0;
        bool        soft    = false;
    };
    std::vector<Cooldown> cooldowns;
    bool                  dismount = false;
    std::string           refuse;

    /// Clamps costs to >= 0, drops empty items / cooldowns and converts to the shared struct
    /// (`activeCooldown` is left 0; the game side fills it).
    [[nodiscard]] Requirement toRequirement() const;
};

[[nodiscard]] bool      testCondition(Call const& condition, Context const& ctx);
[[nodiscard]] Evaluated evaluate(Program const& program, Context const& ctx, Options const& options = {});

/// Remaining blocking cooldown (seconds) of `req` given the active cooldowns: the longest
/// remaining time among its non-soft cooldown ids.
[[nodiscard]] double activeCooldownSeconds(Requirement const& req, std::map<std::string, std::int64_t> const& remainingMs);

} // namespace bs::core::requirements
