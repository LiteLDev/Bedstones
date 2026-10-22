#pragma once

// Opening another unit's screen by name, without depending on that unit (server thread).
//
// Waystones' screens link to each other (selection -> manage -> personal settings -> groups,
// selection header -> edit, edit -> personal settings / modifiers). Each unit registers the
// screens it owns:
//
//     BS_REGISTER_SCREEN_OPENER("manage", [](Player& p, nlohmann::json const& args) { ... });
//
// and any unit opens them with `service::openScreen(player, "manage", args)`. When the owning
// unit is not there (not merged / disabled), `openScreen` returns false and nothing happens.
//
// Screen names and their args (keep in sync with the owning units):
// | name                | owner | args                                                          |
// |---------------------|-------|---------------------------------------------------------------|
// | "selection"         | U10   | {"source": WarpSource json}                                   |
// | "admin_selection"   | U10   | {"owner": "<player uuid>"}                                    |
// | "manage"            | U11   | {"source": WarpSource json}  (return target for "Back")      |
// | "personal_settings" | U11   | {"uid": "<waystone uid>", "source": WarpSource json}          |
// | "manage_groups"     | U12   | {"source": WarpSource json, "from": "<screen to go back to>", "uid": "..."} |
// | "edit_group"        | U12   | {"group": "<group id or empty = new>", "source": ...}        |
// | "edit"              | U13   | {"uid": "<waystone uid>"}                                     |
// | "modifiers"         | U13   | {"uid": "<waystone uid>"}                                     |
// | "inventory_confirm" | U14   | {} (client-local confirm screen is used instead when modded)  |

#include <functional>
#include <string>

#include "nlohmann/json.hpp"

#include "mod/Registry.h"

class Player;

namespace bs::service {

struct ScreenOpener {
    std::string                                                     name;
    std::function<void(Player& player, nlohmann::json const& args)> open;
};
using ScreenOpenerRegistry = StaticRegistry<ScreenOpener>;

/// Opens the screen registered as `name`. Returns false when no unit registered it.
bool openScreen(Player& player, std::string const& name, nlohmann::json const& args = nlohmann::json::object());
/// A unit registered `name`.
[[nodiscard]] bool hasScreen(std::string const& name);

namespace detail {
struct ScreenOpenerRegistrar {
    ScreenOpenerRegistrar(std::string name, std::function<void(Player&, nlohmann::json const&)> fn) {
        ScreenOpenerRegistry::add({std::move(name), std::move(fn)});
    }
};
} // namespace detail

} // namespace bs::service

#define BS_REGISTER_SCREEN_OPENER(name, ...)                                                                           \
    static ::bs::service::detail::ScreenOpenerRegistrar BS_UNIQUE(bs_screen_opener_)(name, __VA_ARGS__)
