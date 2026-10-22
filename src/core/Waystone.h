#pragma once

// The waystone data model (pure C++, no game headers). Mirrors Waystones 21.1.46's
// `Waystone` record and `WaystonesData` player data (docs/SPEC.md §3.1).

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "nlohmann/json.hpp"

namespace bs::core {

/// Block position plus dimension id (0 overworld, 1 nether, 2 end, others as the game reports).
struct GlobalPos {
    int dim = 0;
    int x = 0, y = 0, z = 0;

    friend bool operator==(GlobalPos const&, GlobalPos const&) = default;
    friend auto operator<=>(GlobalPos const&, GlobalPos const&) = default;
    [[nodiscard]] std::string key() const; ///< "dim:x:y:z"
};

/// How a waystone came to exist (`WaystoneOrigin`).
enum class Origin { Unknown, Wilderness, Dungeon, Village, Player };

/// Who can see / use a waystone (`WaystoneVisibility`). Sharestones carry their color in the
/// record's type; their visibility is always `Sharestone`.
enum class Visibility { Activation, Global, Team, ShardOnly, Sharestone };

[[nodiscard]] std::string_view toString(Origin o);
[[nodiscard]] Origin           originFromString(std::string_view s); ///< unknown -> Unknown
[[nodiscard]] std::string_view toString(Visibility v);
[[nodiscard]] std::optional<Visibility> visibilityFromString(std::string_view s);

/// The 16 dye colors in vanilla order (white first). Sharestones use the 15 non-white ones,
/// portstones all 16.
inline constexpr std::array<std::string_view, 16> kDyeColors{
    "white", "orange", "magenta", "light_blue", "yellow", "lime", "pink", "gray",
    "light_gray", "cyan", "purple", "blue", "brown", "green", "red", "black",
};
/// Java DyeColor diffuse colors (0xRRGGBB) in `kDyeColors` order (rune tints).
inline constexpr std::array<std::uint32_t, 16> kDyeDiffuse{
    0xF9FFFE, 0xF9801D, 0xC74EBD, 0x3AB3DA, 0xFED83D, 0x80C71F, 0xF38BAA, 0x474F52,
    0x9D9D97, 0x169C9C, 0x8932B8, 0x3C44AA, 0x835432, 0x5E7C16, 0xB02E26, 0x1D1D21,
};
[[nodiscard]] std::optional<std::size_t> dyeIndex(std::string_view color);

/// Waystone record types (`WaystoneTypes`). Sharestones are "<color>_sharestone".
namespace types {
inline constexpr std::string_view kWaystone         = "waystone";
inline constexpr std::string_view kWarpPlate        = "warp_plate";
inline constexpr std::string_view kWarpPortal       = "warp_portal";
inline constexpr std::string_view kFleetingMemorial = "fleeting_memorial";
[[nodiscard]] bool               isSharestone(std::string_view type);
[[nodiscard]] std::string        sharestone(std::string_view color); ///< "red" -> "red_sharestone"
/// "red_sharestone" -> "red"; nullopt for other types.
[[nodiscard]] std::optional<std::string> sharestoneColor(std::string_view type);
} // namespace types

/// One waystone (any type) known to the world.
struct WaystoneRecord {
    std::string uid;                       ///< random UUID string, never changes
    std::string type{types::kWaystone};    ///< see `types`
    std::string name;                      ///< may be empty (unnamed generated waystone)
    GlobalPos   pos;                       ///< lower block of the waystone
    std::string style;                     ///< block id path, e.g. "mossy_waystone" (for rendering / commands)
    int         facing = 0;                ///< horizontal facing, Java 2D data value: 0 S, 1 W, 2 N, 3 E
    bool        transient = false;         ///< silk-touched item in transit: hidden from global lists
    Origin      origin    = Origin::Unknown;
    std::string ownerUid;                  ///< player UUID string ("" = no owner)
    std::string ownerName;
    Visibility  visibility = Visibility::Activation;
    /// Free-form data owned by feature units (modifiers, plate status, ...). Keys are unit-prefixed.
    nlohmann::json extra = nlohmann::json::object();

    [[nodiscard]] bool hasOwner() const { return !ownerUid.empty(); }
    [[nodiscard]] bool isWaystone() const { return type == types::kWaystone; }
    [[nodiscard]] bool isSharestone() const { return types::isSharestone(type); }
    [[nodiscard]] bool isGlobal() const { return visibility == Visibility::Global; }

    [[nodiscard]] nlohmann::json        toJson() const;
    [[nodiscard]] static WaystoneRecord fromJson(nlohmann::json const& j);
};

/// A user or inbuilt waystone group (Waystones' `GroupRegistry` entry).
struct WaystoneGroup {
    std::string id;        ///< "waystones:favorites" (inbuilt) or "waystones:user/<uuid>"
    std::string name;
    std::string icon;      ///< icon id, e.g. "favorites"
    std::string color;     ///< ChatFormatting name, e.g. "gold"
    bool        inbuilt   = false;
    bool        hidden    = false;
    int         sortIndex = 0;

    [[nodiscard]] nlohmann::json       toJson() const;
    [[nodiscard]] static WaystoneGroup fromJson(nlohmann::json const& j);
};

/// Per-player waystone data (Waystones' `WaystonesData` in the player's persistent NBT).
struct PlayerWaystoneData {
    std::vector<std::string>                        activated;    ///< activated waystone uids (type waystone only)
    std::vector<std::string>                        sortingIndex; ///< manual order of uids
    std::string                                     sortMode = "manual"; ///< manual | name | distance
    std::map<std::string, std::int64_t>             cooldowns;    ///< cooldown id -> expiry (epoch ms)
    std::map<std::string, std::string>              aliases;      ///< uid -> personal name
    std::map<std::string, std::vector<std::string>> groups;       ///< uid -> group ids
    std::set<std::string>                           hidden;       ///< uids hidden from the menus
    std::vector<WaystoneGroup>                      groupRegistry;
    /// Data owned by feature units; unknown keys survive load/save.
    nlohmann::json extra = nlohmann::json::object();

    [[nodiscard]] bool isActivated(std::string_view uid) const;
    /// Adds `uid` (and appends it to the sorting index). Returns false if it was already there.
    bool activate(std::string const& uid);
    /// Removes `uid` from the activated list and the sorting index. Returns false if absent.
    bool forget(std::string const& uid);
    /// Personalized name: the alias if set, otherwise `name`.
    [[nodiscard]] std::string effectiveName(std::string const& uid, std::string const& name) const;

    [[nodiscard]] nlohmann::json            toJson() const;
    [[nodiscard]] static PlayerWaystoneData fromJson(nlohmann::json const& j);
};

/// A random RFC 4122 version 4 UUID string ("xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx").
[[nodiscard]] std::string randomUuid();

/// Glob matching with `*` and `?` (selftest filters).
[[nodiscard]] bool globMatch(std::string_view pattern, std::string_view text);

} // namespace bs::core
