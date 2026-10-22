#pragma once

#include <string>

namespace bs {

/// `config/mod.json`: foundation-wide settings (not Waystones' own options; those live in the
/// sections of the units that own them: general, teleports, inventoryButton, worldGen, client).
struct ModConfig {
    int         version = 1;
    /// Language for server messages and GUI text when a player's locale has no translation
    /// ("en_US", "zh_CN", ...).
    std::string language = "en_US";
    /// Use each player's client locale for GUI text when a translation exists.
    bool        usePlayerLocale = true;
    /// BDS only: require clients to accept the resource pack (textures / names of the blocks).
    bool        requireResourcePack = true;
};

} // namespace bs
