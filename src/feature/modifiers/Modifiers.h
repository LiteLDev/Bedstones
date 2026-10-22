#pragma once

// Warp modifiers (U13): the `modifiers` config, the arrival effects (IArrival) and the Warp
// Modifiers screen ("modifiers") over the record's 5-slot block container (server thread).

#include <string>
#include <vector>

#include "core/Waystone.h"
#include "core/modifiers/Modifiers.h"

class Player;

namespace bs::feature::modifiers {

/// Config section `modifiers` (Waystones' `enableModifiers` kill switch).
struct ModifiersConfig {
    int  version         = 1;
    bool enableModifiers = true;
};

[[nodiscard]] bool enabled();

/// Items of the modifier slots (1-4) of a record's container.
[[nodiscard]] std::vector<core::modifiers::Stack> modifierItems(core::WaystoneRecord const& record);
/// "N modifiers active" of a record.
[[nodiscard]] int activeModifierCount(core::WaystoneRecord const& record);

/// Opens the Warp Modifiers screen of `uid` (only checks the distance, like Waystones).
bool openModifiers(Player& player, std::string const& uid);

/// Max distance (blocks) between the player and the block for the modifiers menu.
inline constexpr double kMaxMenuDistance = 8.0;

} // namespace bs::feature::modifiers
