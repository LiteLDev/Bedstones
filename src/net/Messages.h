#pragma once

// Named JSON message channels between server and client mod (besides screens): state sync
// (activated waystones, cooldowns, inventory-button info, ...) and small client requests.
//
// Server side (both builds):
//     bs::net::sendMessage(player, "cooldowns", json);          // companions only, dropped otherwise
//     BS_REGISTER_CLIENT_MESSAGE("inventory_button.warp", [](Player& p, nlohmann::json const& data) { ... });
//
// Client side (client build, client/net/ClientNet.h):
//     bs::client::net::sendMessage("inventory_button.warp", json);
//     BS_REGISTER_SERVER_MESSAGE("cooldowns", [](nlohmann::json const& data) { ... });
//
// Channel names are "<unit>.<what>". Never trust client data: re-validate on the server.

#include <functional>
#include <string>
#include <string_view>

#include "nlohmann/json.hpp"

#include "mod/Registry.h"

class Player;

namespace bs::net {

/// Sends `data` on `channel` to the player's client mod (no-op without a companion; simulated
/// players go to the test sink as "ServerMessage").
void sendMessage(Player& player, std::string_view channel, nlohmann::json data);
/// Sends to every online player with the client mod.
void broadcastMessage(std::string_view channel, nlohmann::json const& data);

/// Server handler of a client-to-server channel (server thread).
struct ClientMessageHandler {
    std::string                                                    channel;
    std::function<void(Player& player, nlohmann::json const& data)> handle;
};
using ClientMessageRegistry = StaticRegistry<ClientMessageHandler>;

/// Delivers a client message as if it came from `player`'s client (selftests).
void simulateClientMessage(Player& player, std::string_view channel, nlohmann::json const& data);

namespace detail {
struct ClientMessageRegistrar {
    ClientMessageRegistrar(std::string channel, std::function<void(Player&, nlohmann::json const&)> fn) {
        ClientMessageRegistry::add({std::move(channel), std::move(fn)});
    }
};
} // namespace detail

} // namespace bs::net

#define BS_REGISTER_CLIENT_MESSAGE(channel, ...)                                                                       \
    static ::bs::net::detail::ClientMessageRegistrar BS_UNIQUE(bs_client_message_)(channel, __VA_ARGS__)
