#pragma once

// Pure logic of the Twinbound Feather (docs/SPEC.md §3.12): the "touching tips" geometry of the
// linking ritual, the per-player twinning progress and the feather link set stored on items.

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bs::core::feather {

/// Ticks both players must keep touching tips before the feathers link (Java TWINNING_DURATION).
inline constexpr int kTwinningDuration = 20;
/// Reach of the "looking at" ray from the eyes (Java TARGETING_RANGE).
inline constexpr double kTargetingRange = 2.0;
/// Hit box inflation used when picking the partner (Java `getEntityHitResult(..., 0.3f)`).
inline constexpr double kPickInflate = 0.3;
/// Player hit box.
inline constexpr double kPlayerWidth  = 0.6;
inline constexpr double kPlayerHeight = 1.8;
/// Heart particles are spawned every this many ticks while twinning.
inline constexpr int kParticleInterval = 6;

struct Vec {
    double x = 0, y = 0, z = 0;
};

/// Distance along the ray from `eye` in direction `look` (normalised internally) to the player
/// box standing at `feet` (inflated by `kPickInflate`), if it is hit within `range`.
[[nodiscard]] std::optional<double> rayHitPlayer(Vec eye, Vec look, Vec feet, double range = kTargetingRange);

/// Index of the candidate (by feet position) hit first by the ray, -1 if none.
[[nodiscard]] int pickTarget(Vec eye, Vec look, std::vector<Vec> const& candidateFeet, double range = kTargetingRange);

/// Midpoint of two positions plus the heart height offset (Java: +1.1).
[[nodiscard]] Vec heartPos(Vec feetA, Vec feetB);

// ---- link sets (stored on the item as a comma separated list of feather ids) -------------------

[[nodiscard]] std::vector<std::string> parseLinks(std::string_view links);
[[nodiscard]] std::string              formatLinks(std::vector<std::string> const& links);
[[nodiscard]] bool                     hasLink(std::string_view links, std::string_view featherId);
/// `links` with `featherId` added (no duplicates).
[[nodiscard]] std::string withLink(std::string_view links, std::string_view featherId);

/// One feather a player carries.
struct FeatherInfo {
    std::string id;    ///< own feather id ("" = never linked)
    std::string links; ///< comma separated linked feather ids
};
/// Both feathers know each other (Java `areFeathersBesties`).
[[nodiscard]] bool areLinked(FeatherInfo const& a, FeatherInfo const& b);
/// Any pair of the two players' feathers is mutually linked.
[[nodiscard]] bool anyLinked(std::vector<FeatherInfo> const& a, std::vector<FeatherInfo> const& b);

// ---- twinning progress (Java `TwinningProgress`, one entry per using player) -----------------------

class TwinningTracker {
public:
    /// One valid tick of `player` touching tips with `target`; returns the new tick count
    /// (restarts at 1 when the target changed).
    int advance(std::string const& player, std::string const& target);
    /// Drops the entry of `player` (released, not touching, linked).
    void reset(std::string const& player);
    /// Drops every entry whose player is not in `active` (stopped using without an event).
    void retain(std::vector<std::string> const& active);
    [[nodiscard]] int  ticks(std::string const& player) const;
    [[nodiscard]] bool empty() const { return mProgress.empty(); }
    void               clear() { mProgress.clear(); }

private:
    struct Progress {
        std::string target;
        int         ticks = 0;
    };
    std::map<std::string, Progress> mProgress;
};

} // namespace bs::core::feather
