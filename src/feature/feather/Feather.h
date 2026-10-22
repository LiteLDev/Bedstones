#pragma once

// Twinbound Feather (docs/SPEC.md §3.12), server side.
//
// - Linking ritual: two players hold use with a feather (BRUSH animation) while looking at each
//   other within 2 blocks; after 20 ticks the feathers are linked (hearts + enchanting sound).
// - Target provider "feather": while both carry their linked feathers and are online, each is a
//   "player:<uuid>" entry in waystone / warp scroll / warp stone / inventory button menus.
//
// `link()` is the helper behind Waystones' `/waystones twinbound <first> <second>` (owned by the
// commands unit): it links (or gives) feathers without the ritual.

#include <optional>
#include <string>
#include <vector>

#include "core/feather/Feather.h"
#include "game/Items.h"

class ItemStack;
class Player;

namespace bs::feature::feather {

/// The item is a Twinbound Feather.
[[nodiscard]] bool isFeather(ItemStack const& stack);

/// Feather id / links stored on the stack.
[[nodiscard]] core::feather::FeatherInfo infoOf(ItemStack const& stack);

/// Every feather `player` carries (inventory and offhand).
[[nodiscard]] std::vector<core::feather::FeatherInfo> carriedFeathers(Player& player);

/// `a` and `b` are both online and carry mutually linked feathers.
[[nodiscard]] bool arePartners(Player& a, Player& b);

/// The online players `player` is twinbound with right now.
[[nodiscard]] std::vector<Player*> partnersOf(Player& player);

/// The player is holding use with a feather (vanilla item use, or `simulateUse`).
[[nodiscard]] bool isUsingFeather(Player& player);

/// Marks / unmarks a player as holding use with the feather in hand without a client (selftests
/// and the debug partner; it ends when the feather leaves the hand or the link is made).
void simulateUse(Player& player, bool using_);

/// Runs one tick of the linking ritual for every online player (normally driven by the level
/// tick; selftests may call it directly). Returns the number of links made.
int tickRitual();

/// Result of `link()` (Waystones' `commands.waystones.twinbound.*` messages).
enum class LinkResult {
    Success,       ///< "Linked Twinbound Feathers for %s and %s"
    AlreadyLinked, ///< "%s and %s already have linked Twinbound Feathers"
    SamePlayer,    ///< "Choose two different players"
    NoSpace,       ///< "%s needs a free inventory slot for a Twinbound Feather"
    NoFeather,     ///< `give == false` and a player carries no feather
};
struct LinkOutcome {
    LinkResult  result = LinkResult::Success;
    std::string player; ///< NoSpace / NoFeather: the player concerned
};

/// Links a feather of `a` with a feather of `b` (the one in hand, else the first carried one).
/// A player without a feather is given a new one (`give`; NoSpace without a free slot), else the
/// result is NoFeather.
LinkOutcome link(Player& a, Player& b, bool give = true);

/// The id of the target entry of `partner` ("player:<uuid>").
[[nodiscard]] std::string targetIdOf(Player& partner);

} // namespace bs::feature::feather
