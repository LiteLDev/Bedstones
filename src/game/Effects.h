#pragma once

// Feedback helpers (server thread): action bar / chat text, sounds and particles.
//
// Sound names are Bedrock sound event ids ("random.levelup", "mob.endermen.portal", ...);
// particle names are Bedrock particle identifiers ("minecraft:portal_directional", ...).

#include <string>
#include <string_view>

class Player;
class Vec3;

namespace bs::game {

/// Shows `text` above the hotbar (Java's action bar). `§` codes allowed (e.g. "§4" dark red).
void actionBar(Player& player, std::string const& text);
/// Chat line to one player.
void chat(Player& player, std::string const& text);

/// Plays a sound for one player at `at`.
void sound(Player& to, std::string const& name, Vec3 const& at, float volume = 1.f, float pitch = 1.f);
/// Plays a sound for every player in dimension `dim` within `range` blocks of `at`.
void soundAround(int dim, std::string const& name, Vec3 const& at, float volume = 1.f, float pitch = 1.f, float range = 16.f);

/// Spawns one particle effect at `at` in dimension `dim` (visible to nearby players).
void particle(int dim, std::string const& name, Vec3 const& at);
/// Spawns `count` particles at random offsets within +-`spread` blocks around `at`.
void particles(int dim, std::string const& name, Vec3 const& at, int count, float spread);

} // namespace bs::game
