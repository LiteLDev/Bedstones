#pragma once

// Server-authoritative screens (both builds; server thread).
//
// A screen is identified by a string id ("selection", "edit", "manage", ...). The server opens
// it with a JSON model; the client mod renders the native screen registered for that id
// (`BS_REGISTER_CLIENT_SCREEN`, client/ui/screen/Screens.h) and sends actions back. The model is
// whatever the screen needs to draw; `context` is server-only state (source waystone, ...).
//
//     auto session = bs::net::screens::open(player, "selection", model, context);
//     BS_REGISTER_SCREEN_ACTIONS("selection", [](Player& p, ScreenSession& s, std::string const& action,
//                                                 nlohmann::json const& args) { ... });
//
// Players without the client mod get the form fallback registered for the id
// (`BS_REGISTER_SCREEN_FALLBACK`), typically an ll::form Simple/Custom/ModalForm; when there is
// none, `open` returns 0 and nothing is shown.
//
// One screen per player: opening another replaces it (the client swaps screens without closing
// to the world in between). The action "close" is handled here (the player pressed Esc / the
// game dropped the screen) and also reaches the handler so units can react.

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

#include "nlohmann/json.hpp"

#include "mod/Registry.h"

class Player;

namespace bs::net::screens {

struct ScreenSession {
    std::uint32_t  session = 0;
    std::string    id;
    nlohmann::json model   = nlohmann::json::object();
    nlohmann::json context = nlohmann::json::object();
    bool           fallback = false; ///< shown as a form (no client mod)
};

/// Opens screen `id`. Returns the session id (0 when it can't be shown).
std::uint32_t open(Player& player, std::string const& id, nlohmann::json model, nlohmann::json context = nlohmann::json::object());
/// Replaces the open screen's model and sends it (returns false when no screen is open).
bool update(Player& player, nlohmann::json model);
/// Closes the player's screen (also on the client).
void close(Player& player);
/// The player's open screen, nullptr when none.
[[nodiscard]] ScreenSession* current(Player& player);
/// The player's open screen if it is `id` and `session` (0 = any session).
[[nodiscard]] ScreenSession* current(Player& player, std::string const& id, std::uint32_t session = 0);

/// Runs an action as if the player's client sent it (selftests; fallback forms use it too).
void simulateAction(Player& player, std::string const& action, nlohmann::json const& args = nlohmann::json::object());

using ActionHandler =
    std::function<void(Player& player, ScreenSession& session, std::string const& action, nlohmann::json const& args)>;
using FallbackHandler = std::function<void(Player& player, ScreenSession& session)>;

struct ScreenHandlers {
    std::string     id;
    ActionHandler   actions;
    FallbackHandler fallback;
};
using ScreenHandlerRegistry = StaticRegistry<ScreenHandlers>;

namespace detail {
struct ActionRegistrar {
    ActionRegistrar(std::string id, ActionHandler fn) { ScreenHandlerRegistry::add({std::move(id), std::move(fn), {}}); }
};
struct FallbackRegistrar {
    FallbackRegistrar(std::string id, FallbackHandler fn) { ScreenHandlerRegistry::add({std::move(id), {}, std::move(fn)}); }
};
} // namespace detail

} // namespace bs::net::screens

/// Handles client actions of screen `id`: fn(Player&, ScreenSession&, std::string const& action, json const& args).
#define BS_REGISTER_SCREEN_ACTIONS(id, ...)                                                                            \
    static ::bs::net::screens::detail::ActionRegistrar BS_UNIQUE(bs_screen_actions_)(id, __VA_ARGS__)
/// Shows screen `id` to players without the client mod: fn(Player&, ScreenSession&).
#define BS_REGISTER_SCREEN_FALLBACK(id, ...)                                                                           \
    static ::bs::net::screens::detail::FallbackRegistrar BS_UNIQUE(bs_screen_fallback_)(id, __VA_ARGS__)
