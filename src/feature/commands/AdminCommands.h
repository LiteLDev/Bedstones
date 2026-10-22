#pragma once

// Waystones' admin commands (docs/SPEC.md §6), as `/waystones` subcommands (permission level 2):
//
//   activate <targets> <pos>            forget <targets> <pos> | all
//   place <pos> <style> <name...>       count <player>        list <player> [all]
//   gui <player>                        twinbound <first> <second>
//   cooldown <targets> reset all | reset <id> | add <id> <seconds> | set <id> <seconds>=0>
//
// Player arguments are entity selectors, so the targets must be online (like Waystones'
// EntityArgument). The handlers below are the commands' bodies with resolved arguments; the
// command overloads (AdminCommands.cpp) and the selftests call them. `viewer` is the player who
// runs the command (nullptr: console / command block): texts use its locale and `list` measures
// distances from it.

#include <optional>
#include <string>
#include <vector>

#include "core/Waystone.h"
#include "core/commands/AdminLogic.h"

class Player;

namespace bs::feature::commands {

/// Command output: the lines to print and whether the command succeeded.
struct Reply {
    bool                     ok = true;
    std::vector<std::string> lines;

    [[nodiscard]] std::string text() const; ///< lines joined with '\n'
};

Reply activate(Player* viewer, std::vector<Player*> const& targets, core::GlobalPos const& pos);
Reply forget(Player* viewer, std::vector<Player*> const& targets, core::GlobalPos const& pos);
Reply forgetAll(Player* viewer, std::vector<Player*> const& targets);
/// Places the two-tall block `bedstones:<style>` at `pos` (facing north like Java's default
/// state) with a named PLAYER-origin record. Without the block in the behavior pack the record is
/// still created and the reply says so.
Reply place(Player* viewer, core::GlobalPos const& pos, std::string const& style, std::string const& name);
Reply count(Player* viewer, Player& target);
Reply list(Player* viewer, std::optional<core::commands::Viewpoint> const& from, Player& target, bool all);
Reply gui(Player& viewer, Player& target);
Reply twinbound(Player* viewer, Player& first, Player& second);
Reply cooldownResetAll(Player* viewer, std::vector<Player*> const& targets);
Reply cooldownReset(Player* viewer, std::vector<Player*> const& targets, std::string const& id);
Reply cooldownAdd(Player* viewer, std::vector<Player*> const& targets, std::string const& id, int seconds);
Reply cooldownSet(Player* viewer, std::vector<Player*> const& targets, std::string const& id, int seconds);

/// The waystone record at `pos` (lower or upper half), nullptr if none.
[[nodiscard]] core::WaystoneRecord* waystoneAt(core::GlobalPos const& pos);
/// Current wall-clock time in epoch ms (cooldown clock).
[[nodiscard]] std::int64_t nowMs();

namespace detail {
/// Selftests: the item the twinbound command hands out / looks for ("" = bedstones:twinbound_feather).
void setFeatherItemForTest(std::string id);
} // namespace detail

} // namespace bs::feature::commands
