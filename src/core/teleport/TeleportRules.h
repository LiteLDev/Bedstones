#pragma once

// Pure rules of the Waystones teleport pipeline (docs/SPEC.md §3.5): transport modes, the
// landing-spot search order, leash validation, interaction range and teleport-target blocks.
// No game headers: the feature (feature/teleport) feeds these with world data.

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bs::core::teleport {

/// `transportPets` / `transportLeashed` (Waystones' `TransportMobs`).
enum class TransportMode { Enabled, SameDimension, Disabled };

/// Parses "ENABLED" / "SAME_DIMENSION" / "DISABLED" (case-insensitive); `fallback` otherwise.
[[nodiscard]] TransportMode    transportModeFromString(std::string_view s, TransportMode fallback);
[[nodiscard]] std::string_view toString(TransportMode m);
/// Whether entities may come along under `mode` for a warp that changes dimension or not.
[[nodiscard]] bool allowsTransport(TransportMode mode, bool crossDimension);

// ---- landing spot ------------------------------------------------------------------------------

/// Java 2D facing values (0 S, 1 W, 2 N, 3 E).
inline constexpr int kSouth = 0, kWest = 1, kNorth = 2, kEast = 3;
/// Unit offset (dx, dz) of a Java 2D facing.
[[nodiscard]] std::pair<int, int> offsetOf(int facing);
/// Java `Direction.toYRot()` of a 2D facing (S 0, W 90, N 180, E 270).
[[nodiscard]] float yawOf(int facing);

/// The directions tried around a waystone: its facing first, then E, W, S, N.
[[nodiscard]] std::vector<int> landingOrder(int facing);

struct Landing {
    int dx = 0, dz = 0; ///< offset from the target block (0, 0 = on the block itself)
    int facing = 0;     ///< the direction the entity looks (yaw = yawOf(facing))
};
/// Picks the landing spot: the first direction of `landingOrder(facing)` whose block (and the one
/// above) `hasSpace(dx, dz)` reports free; the target position itself when none is (or when
/// `onBlock`, e.g. warp plates). The fallback looks along `facing`.
[[nodiscard]] Landing findLanding(int facing, bool onBlock, std::function<bool(int dx, int dz)> const& hasSpace);

// ---- validation ----------------------------------------------------------------------------------

/// Failure of the leash checks (Waystones' `LeashedWarpDenied` family).
enum class LeashError {
    Disabled,          ///< chat.waystones.cannot_transport_leashed
    DeniedEntity,      ///< chat.waystones.cannot_transport_this_leashed
    DisabledDimension, ///< chat.waystones.cannot_transport_leashed_dimensional
};
/// Validates leashed mobs in Waystones' order: mode DISABLED, then a denied entity, then
/// SAME_DIMENSION across dimensions. `anyDenied` = one of them is on the deny list.
[[nodiscard]] std::optional<LeashError> checkLeashed(TransportMode mode, int leashedCount, bool anyDenied, bool crossDimension);
/// Translation key and English text of a leash error.
[[nodiscard]] std::pair<std::string_view, std::string_view> message(LeashError e);

/// `entityDenyList` match (exact ids; "wither" also matches "minecraft:wither").
[[nodiscard]] bool isDenied(std::vector<std::string> const& denyList, std::string_view entityId);

/// Java `Player.canInteractWithBlock(pos, buffer)`: the distance from the eye to the block's
/// box is below `interactionRange + buffer`.
[[nodiscard]] bool canInteractWithBlock(double eyeX, double eyeY, double eyeZ, int bx, int by, int bz, double interactionRange, double buffer);
/// Non-player entities: within 8 blocks of the block centre (`distanceToSqr <= 64`).
[[nodiscard]] bool entityNearBlock(double x, double y, double z, int bx, int by, int bz);
/// Java block interaction range (survival 4.5, creative 5).
[[nodiscard]] inline double interactionRange(bool creative) { return creative ? 5.0 : 4.5; }
/// Waystones' buffer for source waystones and open menus (`canInteractWithBlock(pos, 4)`).
inline constexpr double kSourceBuffer = 4.0;

// ---- targets -------------------------------------------------------------------------------------

/// The block path (without "bedstones:") that should stand at a record of `type` / `style`.
[[nodiscard]] std::string expectedBlockPath(std::string_view type, std::string_view style);
/// Waystones' `is_teleport_target` block tag, for a "bedstones:" block path: the waystone styles,
/// sharestones, warp plates, warp portals and fleeting memorials (portstones are not targets).
[[nodiscard]] bool isTeleportTargetPath(std::string_view path);

} // namespace bs::core::teleport
