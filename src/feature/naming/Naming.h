#pragma once

// Waystones' name generator as the INameGenerator seam (U9, docs/SPEC.md §3.1 / §3.11).
//
// `service::get<INameGenerator>().generate(origin, biome)`:
// - generated waystones (origin WILDERNESS / VILLAGE / DUNGEON) use `general.nameGenerationMode`;
// - everything else (PLAYER / UNKNOWN: warp plates) uses RANDOM_ONLY, like Waystones' warp plates.
// Every name handed out is remembered per world (`bs:names`, seeded with the names of existing
// records on load) and duplicates get " II", " III", ...

#include <string>

#include "core/Waystone.h"
#include "core/naming/NameGenerator.h"

namespace bs::feature::naming {

/// The mode `generate(origin, ...)` uses for `origin`.
[[nodiscard]] core::naming::Mode modeFor(core::Origin origin);
/// Generates (and remembers) a unique name with an explicit mode.
[[nodiscard]] std::string generate(core::naming::Mode mode, std::string const& biome);
/// The world's used-names set.
[[nodiscard]] bool isUsed(std::string const& name);
/// Removes a name from the used set (tests).
void forget(std::string const& name);

} // namespace bs::feature::naming
