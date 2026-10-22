#pragma once

// Client visuals of waystones (pure logic, no game headers): which records the server syncs to
// a companion for rendering, rune colours, rune quad placement and the floating warp stone
// animation of sharestones (docs/SPEC.md §1.2-1.4).

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "nlohmann/json.hpp"

#include "core/Waystone.h"

namespace bs::core::visuals {

/// Records farther than this (blocks, horizontal + vertical) from the player are not synced.
inline constexpr double kSyncRange = 128.0;
/// Ambient portal / enchant particles only for activated waystones this close (Java animateTick
/// runs for blocks within ~16 blocks of the camera).
inline constexpr double kAmbientRange = 16.0;
/// Runes / floating items are drawn up to this distance.
inline constexpr double kRenderRange = 64.0;
/// Chance per client tick of one ambient particle pair (Java: `random.nextFloat() < 0.75`).
inline constexpr float kAmbientChance = 0.75f;

/// Default rune tint (white) and the two styles with their own colour.
inline constexpr std::uint32_t kRuneWhite     = 0xFFFFFF;
inline constexpr std::uint32_t kRuneBlackstone = 0x993333;
inline constexpr std::uint32_t kRuneEndStone   = 0x7200FF;

/// One record as the client needs it for rendering (channel `visuals.waystones`).
struct Entry {
    std::string uid;
    GlobalPos   pos;
    std::string type;  ///< record type ("waystone", "red_sharestone", ...)
    std::string style; ///< block path ("mossy_waystone", ...)
    int         facing    = 0;
    bool        activated = false; ///< by the player the payload is for

    friend bool operator==(Entry const&, Entry const&) = default;
};

/// Record types the client renders something for (waystones and sharestones).
[[nodiscard]] bool isRendered(std::string_view type);
/// `pos` is within `range` blocks (euclidean, block centres) of (x, y, z) in dimension `dim`.
[[nodiscard]] bool inRange(GlobalPos const& pos, int dim, double x, double y, double z, double range);

/// The rune tint of a waystone style (white, Black Waystone 0x993333, Ender Waystone 0x7200FF).
[[nodiscard]] std::uint32_t waystoneRuneColor(std::string_view style);
/// The dye diffuse colour of "red_sharestone" / "red_portstone" / "red" (white when unknown).
[[nodiscard]] std::uint32_t dyeRuneColor(std::string_view typeOrColor);
/// Rune tint of an entry: style colour for waystones, dye colour for sharestones.
[[nodiscard]] std::uint32_t runeColor(Entry const& e);

/// Waystones show runes only when activated by the viewer; sharestones always.
[[nodiscard]] bool showsRunes(Entry const& e);
/// Ambient portal / enchant particles: activated waystones only.
[[nodiscard]] bool hasAmbientParticles(Entry const& e);
/// Sharestones carry the floating warp stone.
[[nodiscard]] bool hasFloatingItem(Entry const& e);

/// Builds the entries of the payload: `records` filtered to rendered types, non-transient,
/// within `range` of the player, with the activation flag from `activated(uid)`.
template <class Records, class Activated>
[[nodiscard]] std::vector<Entry>
select(Records const& records, int dim, double x, double y, double z, double range, Activated const& activated) {
    std::vector<Entry> out;
    for (WaystoneRecord const* r : records) {
        if (!r || r->transient || !isRendered(r->type) || !inRange(r->pos, dim, x, y, z, range)) continue;
        out.push_back(Entry{r->uid, r->pos, r->type, r->style, r->facing, r->isWaystone() && activated(r->uid)});
    }
    return out;
}

[[nodiscard]] nlohmann::json     toJson(std::vector<Entry> const& entries);
[[nodiscard]] std::vector<Entry> fromJson(nlohmann::json const& payload);

// ---- geometry -----------------------------------------------------------------------------------

/// Rune sets in the rune atlas (`textures/bs_visuals/runes`, 8x16 cells side by side).
enum class RuneKind { Waystone = 0, Sharestone = 1, Portstone = 2 };
inline constexpr int kRuneCellWidth = 8;

/// A flat rune quad on one side of a two-tall stone, facing outwards.
struct RuneQuad {
    double x, y, z; ///< centre (world)
    double nx, nz;  ///< outward normal
};
/// Distance of the rune quads from the block column's axis (the 10-wide middle of the model,
/// plus a small gap against z-fighting).
inline constexpr double kRuneInset = 5.0 / 16.0 + 0.01;
/// Centre height above the lower block (the rune band spans y 9/16 .. 24/16).
inline constexpr double kRuneCentreY = 16.5 / 16.0;
/// The four rune quads (S, W, N, E sides) of the two-tall stone whose lower block is at `pos`.
[[nodiscard]] std::array<RuneQuad, 4> runeQuads(GlobalPos const& pos);

/// Pose of the floating warp stone above a sharestone at render time `ticks` (game time +
/// partial tick): angle = (ticks % 720) / 2 degrees, bob = sin(ticks / 8) * 0.025 blocks.
struct FloatPose {
    double angleDeg;
    double bob;
};
[[nodiscard]] FloatPose floatingPose(std::uint64_t gameTime, double partialTick);
/// Base height of the floating item's centre above the lower block (one block above the
/// sharestone's top, as Waystones draws it at y+1 of the upper half, lifted by half its size).
inline constexpr double kFloatBaseY = 2.25;
/// Item scale (Java: 0.5).
inline constexpr double kFloatScale = 0.5;

} // namespace bs::core::visuals
