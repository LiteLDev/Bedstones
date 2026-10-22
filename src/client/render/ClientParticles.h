#pragma once

// Client-only particle effects (client build): spawned straight into the client level's particle
// engine, so only the local player sees them (nothing is sent to the server or other players).
// Molang variables are passed to the effect as `variable.<name>` (e.g. "bs_r" -> `v.bs_r`).

#include <initializer_list>
#include <string_view>
#include <utility>

class Level;
class Vec3;

namespace bs::client::render {

using MolangVar = std::pair<char const*, float>;

/// Spawns particle effect `effect` (e.g. "bedstones:visuals_rune") at `at` in the client level.
void spawnLocal(Level& clientLevel, std::string_view effect, Vec3 const& at, std::initializer_list<MolangVar> vars = {});

} // namespace bs::client::render
