#pragma once

// Config section `general` (file config/general.json), owned by U9.
//
// Waystones 21.1.46 keeps the name generation settings in its `worldGen` section; Bedstones keeps
// them here with the other access / naming rules (U15 owns `worldGen` for placement only).

#include <string>
#include <vector>

namespace bs::feature::access {

struct GeneralConfig {
    int version = 1;
    /// Origins whose waystones are restricted (PLAYER: only the owner may edit owned waystones;
    /// other origins: only creative players may edit waystones of that origin).
    /// Values: PLAYER, WILDERNESS, VILLAGE, DUNGEON, UNKNOWN.
    std::vector<std::string> restrictedWaystones{"PLAYER"};
    /// Visibility of new waystones: ACTIVATION, GLOBAL or TEAM.
    std::string defaultVisibility = "ACTIVATION";
    /// Lets every player make waystones global (otherwise creative players only).
    bool allowEveryoneToManageGlobalWaystones = false;
    /// Deprecated in Waystones 21.1: GLOBAL here also lets everyone manage global waystones.
    std::vector<std::string> allowedVisibilities{};
    /// PRESET_FIRST, RANDOM_ONLY, PRESET_ONLY or MIXED.
    std::string nameGenerationMode = "PRESET_FIRST";
    /// Placeholders: {MrPork} (random syllable name), {Biome} (biome name).
    std::string nameGenerationTemplate = "{MrPork}";
    /// Names used by the PRESET_* / MIXED modes.
    std::vector<std::string> nameGenerationPresets{};
};

} // namespace bs::feature::access
