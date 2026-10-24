#pragma once

// Pure warp plate rules (docs/SPEC.md §1.5): galactic identifier and colour, use time, status
// and target choice. No game headers; unit-tested in tests/WarpPlateTest.cpp.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bs::core::warpplate {

/// Ticks a dormant shard needs to attune.
inline constexpr int kAttunementTicks = 30;

/// Java's `String.hashCode()` (UTF-16 code units; identifiers are ASCII).
[[nodiscard]] std::int32_t javaHash(std::string_view s);

/// The plate's galactic identifier: the uid without digits and dashes, first 8 characters.
[[nodiscard]] std::string galacticIdentifier(std::string_view uid);

/// `text` in the Standard Galactic glyphs of the resource pack's font page E1 (font/glyph_E1.png,
/// tools/textures/galactic.ps1): letters a-z become U+E161..U+E17A (UTF-8), other characters stay.
[[nodiscard]] std::string galacticText(std::string_view text);

/// `|hash| % 15` of the identifier (0..14; Java's Math.abs keeps INT_MIN negative -> 7, gray).
[[nodiscard]] int colorIndex(std::string_view identifier);

/// The ChatFormatting id (0..15, also the `§` code digit) for a colour index, with Waystones'
/// remapping GRAY -> LIGHT_PURPLE, DARK_GRAY -> DARK_PURPLE, BLACK -> GOLD.
[[nodiscard]] int chatFormattingId(int colorIndex);
/// `§` colour code character of a ChatFormatting id ('0'..'9', 'a'..'f').
[[nodiscard]] char formattingCode(int formattingId);
/// RGB (0xRRGGBB) of a ChatFormatting id (vanilla text colours).
[[nodiscard]] std::uint32_t formattingRgb(int formattingId);

/// Use time in ticks: each amethyst shard -1.6%, each slime ball +1.6% (counted as items),
/// truncated, clamped to [1, 2 * configured].
[[nodiscard]] int useTime(int configured, int amethystShards, int slimeBalls);

/// What sits in the shard slot (slot 0).
enum class ShardKind { None, Dormant, Attuned, Crumbling, Deepslate, Other };
[[nodiscard]] ShardKind shardKindOf(std::string_view itemId);
[[nodiscard]] bool      isWarpShard(std::string_view itemId);

/// Same values as `ids::WarpPlateStatus` / the block state.
enum class Status { Empty = 0, Idle, Attuning, Warping, WarpingInvalid, RedstoneDisabled, Locked };

/// The resting status (Waystones' `getIdleState`).
[[nodiscard]] Status idleStatus(ShardKind shardSlot, bool redstoneDisabled);
/// The plate can have a target (a shard that is neither missing nor deepslate).
[[nodiscard]] bool hasPotentialTarget(ShardKind shardSlot);

/// One attuned shard in the container that points elsewhere.
struct Candidate {
    int         slot      = 0;
    bool        singleUse = false; ///< crumbling shard
    std::string target;            ///< attunement uid
};

/// Picks the target among `candidates` (Waystones' `getTargetAttunementStack`): spider eye keeps
/// only single-use shards when there are any; quartz is round robin over `lastSlot`, which is
/// advanced on every call; otherwise `random(n)` (returns 0..n-1) decides.
template <class Random>
[[nodiscard]] std::optional<Candidate>
chooseTarget(std::vector<Candidate> candidates, bool roundRobin, bool preferSingleUse, int& lastSlot, Random&& random) {
    if (preferSingleUse) {
        bool any = false;
        for (auto const& c : candidates) any = any || c.singleUse;
        if (any) std::erase_if(candidates, [](Candidate const& c) { return !c.singleUse; });
    }
    if (candidates.empty()) return std::nullopt;
    int n    = static_cast<int>(candidates.size());
    lastSlot = ((lastSlot + 1) % n + n) % n;
    if (roundRobin) return candidates[static_cast<std::size_t>(lastSlot)];
    int r = random(n);
    if (r < 0 || r >= n) r = 0;
    return candidates[static_cast<std::size_t>(r)];
}

} // namespace bs::core::warpplate
