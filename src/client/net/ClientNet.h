#pragma once

// Client side of the companion protocol (see net/Packets.h). Client thread only.

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

#include "nlohmann/json.hpp"

#include "mod/Registry.h"

namespace bs::client::net {

/// The server answered our Hello with a compatible Welcome (this connection).
[[nodiscard]] bool serverReady();

/// Sends a client-to-server message on `channel` (dropped until the server is ready).
void sendMessage(std::string_view channel, nlohmann::json data);

// ---- server screens -------------------------------------------------------------------------------

/// The screen the server opened for us.
struct ServerScreen {
    std::uint32_t  session = 0;
    std::string    id;
    nlohmann::json model = nlohmann::json::object();
};
/// The open server screen (nullptr when none).
[[nodiscard]] ServerScreen const* serverScreen();
/// Bumps on every open / update / close.
[[nodiscard]] std::uint64_t screenRevision();
/// Sends an action of the open server screen (`session` is added).
void sendScreenAction(std::string const& action, nlohmann::json args = nlohmann::json::object());
/// The player closed the server screen (Esc): tells the server and forgets it.
void closeServerScreen();

// ---- message handlers ---------------------------------------------------------------------------------

struct ServerMessageHandler {
    std::string                                 channel;
    std::function<void(nlohmann::json const&)> handle;
};
using ServerMessageRegistry = StaticRegistry<ServerMessageHandler>;

namespace detail {
struct ServerMessageRegistrar {
    ServerMessageRegistrar(std::string channel, std::function<void(nlohmann::json const&)> fn) {
        ServerMessageRegistry::add({std::move(channel), std::move(fn)});
    }
};
} // namespace detail

/// Called when the client leaves a world (reset per-world client state). Static-init registration.
void onLeaveWorld(std::function<void()> fn);

} // namespace bs::client::net

/// Handles a server-to-client message channel on the client: fn(nlohmann::json const& data).
#define BS_REGISTER_SERVER_MESSAGE(channel, ...)                                                                       \
    static ::bs::client::net::detail::ServerMessageRegistrar BS_UNIQUE(bs_server_message_)(channel, __VA_ARGS__)
