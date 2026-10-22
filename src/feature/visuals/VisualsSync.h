#pragma once

// Server side of the waystone visuals (U17): sends each companion the waystone / sharestone
// records near it, with its own activation flag, on channel `visuals.waystones`. The client
// (client/render/WaystoneVisuals.cpp) draws runes, ambient particles and the floating warp stone
// from it.
//
// Payload: {"entries": [{"uid", "dim", "x", "y", "z", "type", "style", "facing", "activated"}]}
// (core/visuals/Visuals.h). Sent within 2 s of a companion appearing (login / world ready: the
// periodic check finds no payload sent yet), on the next tick when records or the player's
// activations change, and when the set near the player changes as it moves (checked every 2 s).
// Unchanged payloads are not re-sent.

#include <string_view>
#include <vector>

#include "core/visuals/Visuals.h"

class Player;

namespace bs::feature::visuals {

inline constexpr std::string_view kChannel = "visuals.waystones";
/// Ticks between the movement re-checks.
inline constexpr int kRefreshTicks = 40;

/// The entries the player's client gets now (records within `kSyncRange` of the player).
[[nodiscard]] std::vector<core::visuals::Entry> entriesFor(Player& player);
/// Sends the player's payload if it differs from the last one sent (always with `force`).
/// Returns true if something was sent. No-op for players without the client mod.
bool sync(Player& player, bool force = false);

} // namespace bs::feature::visuals
