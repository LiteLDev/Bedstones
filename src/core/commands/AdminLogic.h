#pragma once

// Pure logic of the `/waystones` admin commands (docs/SPEC.md §6): cooldown ids and maths,
// the admin inspection order of `list` / `gui`, dimension names and Twinbound Feather links.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Waystone.h"

namespace bs::core::commands {

/// Cooldown ids are resource locations; a bare path gets the "waystones:" namespace
/// ("inventory_button" -> "waystones:inventory_button"). Lower-cased.
[[nodiscard]] std::string normalizeCooldownId(std::string_view id);

/// `cooldown add`: extends from the later of now and the current expiry.
[[nodiscard]] std::int64_t cooldownAdd(std::int64_t nowMs, std::optional<std::int64_t> current, int seconds);
/// `cooldown set`: now + seconds.
[[nodiscard]] std::int64_t cooldownSet(std::int64_t nowMs, int seconds);

/// Java dimension id of a Bedrock dimension ("minecraft:overworld", "minecraft:the_nether",
/// "minecraft:the_end", otherwise "minecraft:dimension_<n>").
[[nodiscard]] std::string dimensionName(int dim);

/// Euclidean distance from (x, y, z) to the centre of block `pos`, truncated like Java's `(int)`.
[[nodiscard]] int blockDistance(double x, double y, double z, GlobalPos const& pos);

/// One activated waystone of the inspected player.
struct ListEntry {
    std::string uid;
    std::string name; ///< effective (personalized) name
    GlobalPos   pos;
    bool        owned = false;
};

/// The caller's position (the console has none).
struct Viewpoint {
    int    dim = 0;
    double x = 0, y = 0, z = 0;
};

/// Waystones' `forAdminInspection` order (stable): owned first, then waystones in the caller's
/// dimension, then by squared distance to the caller.
void sortForAdminInspection(std::vector<ListEntry>& entries, std::optional<Viewpoint> const& caller);

/// Both feathers have ids and each links to the other.
[[nodiscard]] bool feathersLinked(
    std::optional<std::string> const& firstId,
    std::optional<std::string> const& firstLink,
    std::optional<std::string> const& secondId,
    std::optional<std::string> const& secondLink
);

} // namespace bs::core::commands
