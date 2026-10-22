#pragma once

// Service seams: the points where feature units plug the real Waystones behaviour in.
//
// Each seam has a minimal default implementation in the foundation (service/Defaults.cpp), so
// every unit builds, runs and merges on its own. A unit that owns a seam replaces the default in
// its module `install()` with `service::set<ISeam>(std::make_unique<MyImpl>())` and restores it
// with `service::reset<ISeam>()` in `uninstall()`. Callers always go through `service::get<ISeam>()`.
//
// | Seam            | Owner unit | Default                                              |
// |-----------------|------------|------------------------------------------------------|
// | ITeleport       | U7         | plain teleport to the block in front of the target   |
// | IRequirements   | U8         | everything free, nothing consumed                    |
// | IPermissions    | U9         | owner or creative may edit; visibility = activation  |
// | INameGenerator  | U9         | "Waystone <n>"                                       |
// | ITargets        | U10        | activated + global waystones + providers             |
// | ISelection      | U10        | SimpleForm list                                      |
// | IEditScreen     | U13        | CustomForm with name + visibility                    |
// | IArrival        | U13        | nothing (modifier effects on arriving entities)      |
// | IGroups         | U12        | inbuilt groups + the player's user groups            |
//
// Extra list entries (Twinbound players, memorials, the return portal) are contributed with
// `BS_REGISTER_TARGET_PROVIDER` (service/TargetProviders.h), not by replacing ITargets.
//
// Server thread only.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Warp.h"
#include "core/Waystone.h"

class Player;
class Actor;

namespace bs::service {

// ---- warps --------------------------------------------------------------------------------------

/// Where a warp lands (resolved target).
struct Destination {
    core::GlobalPos block;     ///< the target block (waystone lower half) or the exact block for players
    float           x = 0, y = 0, z = 0; ///< feet position
    float           yaw = 0;
    std::string     targetUid;  ///< waystone uid when the target is a waystone record ("" otherwise)
};

struct WarpRequest {
    core::WarpSource source;
    std::string      targetId;          ///< waystone uid or a provider id (see TargetProvider)
    bool             skipRequirements = false; ///< admin / command warps
};

struct WarpResult {
    bool        ok = false;
    std::string error; ///< translation key of the failure message (shown dark red in the action bar)
    std::string errorFallback;
    [[nodiscard]] static WarpResult success() { return {true, {}, {}}; }
    [[nodiscard]] static WarpResult fail(std::string key, std::string fallback) { return {false, std::move(key), std::move(fallback)}; }
};

class ITeleport {
public:
    virtual ~ITeleport() = default;
    /// Validates, pays requirements and teleports `player` (plus whatever the implementation
    /// takes along). Shows errors to the player itself; also returns them. Implementations must
    /// honour service/WarpEvents.h: `intercept()` before paying, `notifyWarped()` after success,
    /// plus the target provider's `onWarped` and `IArrival::onArrived`.
    virtual WarpResult warp(Player& player, WarpRequest const& request) = 0;
    /// Resolves a target id to a landing spot (nullopt: gone / invalid).
    [[nodiscard]] virtual std::optional<Destination> resolve(Player& player, std::string const& targetId) = 0;
};

class IRequirements {
public:
    virtual ~IRequirements() = default;
    /// The requirements of warping `player` from `source` to `target` (a list entry).
    [[nodiscard]] virtual core::Requirement resolve(Player& player, core::WarpSource const& source, core::WarpTarget const& target) = 0;
    /// The player can pay (creative players always can).
    [[nodiscard]] virtual bool canAfford(Player& player, core::Requirement const& requirement) = 0;
    /// Pays and starts cooldowns.
    virtual void consume(Player& player, core::Requirement const& requirement) = 0;
};

// ---- access / naming -----------------------------------------------------------------------------

class IPermissions {
public:
    virtual ~IPermissions() = default;
    /// nullopt = may edit; otherwise the translation key (+ fallback) of the reason.
    struct Denied {
        std::string key;
        std::string fallback;
    };
    [[nodiscard]] virtual std::optional<Denied> canEdit(Player const& player, core::WaystoneRecord const& record) = 0;
    [[nodiscard]] virtual bool canManageGlobal(Player const& player) = 0;
    /// Visibility choices offered in the edit screen, in cycle order (first = default).
    [[nodiscard]] virtual std::vector<core::Visibility> visibilityOptions(Player const& player, core::WaystoneRecord const& record) = 0;
    /// Default visibility of a new player-placed waystone.
    [[nodiscard]] virtual core::Visibility defaultVisibility() = 0;
    /// Whether `player` can see / list `record` at all (activation, global, team, sharestone rules).
    [[nodiscard]] virtual bool canSee(Player const& player, core::WaystoneRecord const& record) = 0;
};

class INameGenerator {
public:
    virtual ~INameGenerator() = default;
    /// A fresh unique name for a generated waystone (`biome` may be empty).
    [[nodiscard]] virtual std::string generate(core::Origin origin, std::string const& biome) = 0;
};

// ---- menus -----------------------------------------------------------------------------------------

class ITargets {
public:
    virtual ~ITargets() = default;
    /// The entries a selection menu opened from `source` lists for `player` (docs/SPEC.md §3.7),
    /// with requirements resolved, unsorted. Includes the source waystone flagged `current`.
    [[nodiscard]] virtual std::vector<core::WarpTarget> list(Player& player, core::WarpSource const& source) = 0;
};

class ISelection {
public:
    virtual ~ISelection() = default;
    /// Opens the waystone selection menu for `source`.
    virtual void open(Player& player, core::WarpSource const& source) = 0;
    /// `/waystones gui <player>`: `viewer` browses `ownerUuid`'s waystones (admin screen).
    virtual void openAdmin(Player& viewer, std::string const& ownerUuid) = 0;
};

class IEditScreen {
public:
    virtual ~IEditScreen() = default;
    /// Opens the Edit Waystone screen for `uid` (read-only when editing is denied).
    virtual void open(Player& player, std::string const& uid) = 0;
};

class IArrival {
public:
    virtual ~IArrival() = default;
    /// An entity arrived at `target` (apply the target block's modifier effects).
    virtual void onArrived(Actor& entity, core::WaystoneRecord const& target) = 0;
};

// ---- groups -------------------------------------------------------------------------------------------

/// Inbuilt group ids (docs/SPEC.md §4.4). Dimension groups are "waystones:dimension/<dim id>"
/// (0 overworld, 1 nether, 2 end); user groups "waystones:user/<uuid>".
namespace groups {
inline constexpr std::string_view kFavorites = "waystones:favorites";
inline constexpr std::string_view kPlayers   = "waystones:players";
inline constexpr std::string_view kGlobal    = "waystones:global";
inline constexpr std::string_view kTeams     = "waystones:teams";
[[nodiscard]] inline std::string dimension(int dim) { return "waystones:dimension/" + std::to_string(dim); }
} // namespace groups

class IGroups {
public:
    virtual ~IGroups() = default;
    /// Every group the player has (inbuilt + user), sorted by sortIndex then name; hidden included
    /// (callers skip `hidden` ones where Waystones does).
    [[nodiscard]] virtual std::vector<core::WaystoneGroup> groups(Player& player) = 0;
    /// Group ids a list entry belongs to: the player's explicit assignment (favorites, user groups)
    /// plus the implicit ones (global, players, teams, its dimension).
    [[nodiscard]] virtual std::vector<std::string> groupsOf(Player& player, core::WarpTarget const& target) = 0;
};

// ---- access -----------------------------------------------------------------------------------------

namespace detail {
template <class I>
std::unique_ptr<I>& slot() {
    static std::unique_ptr<I> s;
    return s;
}
template <class I>
std::unique_ptr<I> makeDefault();
} // namespace detail

/// The active implementation of seam `I` (the default when none was set).
template <class I>
I& get() {
    auto& s = detail::slot<I>();
    if (!s) s = detail::makeDefault<I>();
    return *s;
}
/// Replaces seam `I` (unit `install()`).
template <class I>
void set(std::unique_ptr<I> impl) {
    detail::slot<I>() = std::move(impl);
}
/// Restores the default of seam `I` (unit `uninstall()`).
template <class I>
void reset() {
    detail::slot<I>().reset();
}

// Defaults (service/Defaults.cpp).
namespace detail {
template <>
std::unique_ptr<ITeleport> makeDefault<ITeleport>();
template <>
std::unique_ptr<IRequirements> makeDefault<IRequirements>();
template <>
std::unique_ptr<IPermissions> makeDefault<IPermissions>();
template <>
std::unique_ptr<INameGenerator> makeDefault<INameGenerator>();
template <>
std::unique_ptr<ITargets> makeDefault<ITargets>();
template <>
std::unique_ptr<ISelection> makeDefault<ISelection>();
template <>
std::unique_ptr<IEditScreen> makeDefault<IEditScreen>();
template <>
std::unique_ptr<IArrival> makeDefault<IArrival>();
template <>
std::unique_ptr<IGroups> makeDefault<IGroups>();
} // namespace detail

} // namespace bs::service
