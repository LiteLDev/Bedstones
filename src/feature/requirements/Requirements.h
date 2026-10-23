#pragma once

// U8: warp requirements and cooldowns (docs/SPEC.md §3.6). Replaces the IRequirements seam.
//
// Cooldowns live in `PlayerWaystoneData::cooldowns` (cooldown id -> expiry epoch ms; the shared
// contract) and are synced to the client mod on the message channel
//
//     "requirements.cooldowns"          server -> client  {"<id>": remainingMs, ...}
//                                        (all active cooldowns; sent after the handshake, on
//                                        every change and on request)
//     "requirements.cooldowns.request"  client -> server  {}  (answers with the message above)
//
// Any client unit (inventory button, selection screen) can subscribe to the first channel with
// BS_REGISTER_SERVER_MESSAGE; every registered handler receives it.

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "core/Warp.h"
#include "core/requirements/Requirements.h"

class Player;

namespace bs::feature::requirements {

/// Config section `requirements` (Waystones' teleports.enableCosts / enableCooldowns /
/// warpRequirements).
struct RequirementsConfig {
    int                      version          = 1;
    bool                     enableCosts      = true;
    bool                     enableCooldowns  = true;
    std::vector<std::string> warpRequirements = core::requirements::defaultRules();
};

inline constexpr std::string_view kCooldownChannel        = "requirements.cooldowns";
inline constexpr std::string_view kCooldownRequestChannel = "requirements.cooldowns.request";
inline constexpr std::string_view kInventoryButtonCooldown = "waystones:inventory_button";

// ---- cooldowns -----------------------------------------------------------------------------------

[[nodiscard]] std::int64_t nowMs();
/// Active cooldowns of `player` (id -> ms left). Expired entries are pruned from the player data.
[[nodiscard]] std::map<std::string, std::int64_t> activeCooldowns(Player& player);
/// Sets cooldown `id` to expire `seconds` from now (<= 0 removes it). Persists and syncs.
void setCooldown(Player& player, std::string const& id, double seconds);
/// Removes cooldown `id` ("" = all). Persists and syncs.
void resetCooldowns(Player& player, std::string const& id = {});
/// Sends the active cooldowns to the player's client mod.
void syncCooldowns(Player& player);

#ifdef BS_CLIENT
/// Client mod: time left of a cooldown the server synced (0 when none).
[[nodiscard]] std::int64_t clientCooldownRemainingMs(std::string const& id);
#endif

// ---- experience (Java semantics on Bedrock's level + progress attributes) ----------------------

[[nodiscard]] int playerLevel(Player& player);
/// Total experience points (levels converted with the Java formulas + progress).
[[nodiscard]] int totalXp(Player& player);
/// Sets the total experience points (level and progress).
void setTotalXp(Player& player, int total);

// ---- display -------------------------------------------------------------------------------------

/// The requirement tooltip lines of docs/SPEC.md §4.1 in the player's language, with § colors:
/// "Experience Cost: N" / "Level Cost: N" (green), "Cooldown: N seconds" (gold, the time left when
/// a cooldown is active), "Item Cost: N item", the refuse message (red; "$key" is translated).
/// Keys: tooltip.waystones.{xp,level,cooldown,item}_requirement.
[[nodiscard]] std::vector<std::string> tooltipLines(Player const& player, core::Requirement const& requirement);

// ---- evaluation ----------------------------------------------------------------------------------

/// The parsed configured rule list (re-parsed when the config changes).
[[nodiscard]] core::requirements::Program const& program();
/// The evaluation context of a warp.
[[nodiscard]] core::requirements::Context
buildContext(Player& player, core::WarpSource const& source, core::WarpTarget const& target);

} // namespace bs::feature::requirements
