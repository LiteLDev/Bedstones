#pragma once

// Companion protocol: LeviLamina runtime packets between the server mod and the client mod.
//
// Every packet carries the protocol version (u16) and a JSON body. A peer drops packets of
// another version. Packets are only ever sent to a client that announced the companion
// (`HelloPacket`), so vanilla clients never see them. A LeviLamina server without the mod drops
// the client's runtime packets (unknown runtime id); the client sends `Hello` at most 15 times
// and nothing else until `Welcome`. Client-to-server bodies are limited to `kMaxClientBody`.
//
// Directions:
//   client -> server: Hello, ScreenAction, ClientMessage
//   server -> client: Welcome, ScreenOpen, ScreenUpdate, ScreenClose, ServerMessage
//
// Bodies (JSON):
//   Hello / Welcome  {"version": "<mod version>", "features": [..]}
//   ScreenOpen       {"session": n, "id": "<screen id>", "model": {..}}       (net/Screens.h)
//   ScreenUpdate     {"session": n, "model": {..}}                             (full model)
//   ScreenClose      {"session": n}
//   ScreenAction     {"session": n, "action": "...", "args": {..}}
//   ServerMessage    {"channel": "...", "data": ..}                            (net/Messages.h)
//   ClientMessage    {"channel": "...", "data": ..}

#include <string>

#include "ll/api/network/packet/Packet.h"

#include "mc/deps/core/utility/BinaryStream.h"
#include "mc/deps/core/utility/ReadOnlyBinaryStream.h"

#include "nlohmann/json.hpp"


namespace bs::net {

/// Bump when a packet body changes incompatibly.
inline constexpr int kProtocolVersion = 1;

/// Largest JSON body accepted from the server (screen models with hundreds of entries are far below).
inline constexpr std::uint64_t kMaxBody = 4u * 1024u * 1024u;
/// Largest JSON body accepted from a client (actions are tiny; keeps parsing cheap).
inline constexpr std::uint64_t kMaxClientBody = 16u * 1024u;

/// A runtime packet with a protocol version and a JSON body of at most `MaxBody` bytes.
template <class Derived, std::uint64_t MaxBody = kMaxBody>
class JsonPacket : public ll::network::PacketBase<Derived> {
public:
    int            version = kProtocolVersion;
    nlohmann::json body    = nlohmann::json::object();

    JsonPacket() = default;
    explicit JsonPacket(nlohmann::json b) : body(std::move(b)) {}

    void write(BinaryStream& stream) const override {
        stream.writeUnsignedShort(static_cast<ushort>(version), "version", "");
        stream.writeString(body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace), "body", "");
    }

    [[nodiscard]] Bedrock::Result<void> read(ReadOnlyBinaryStream& stream) override {
        auto v = stream.getUnsignedShort();
        if (!v) return nonstd::make_unexpected(v.error());
        version = v.value();
        auto s  = stream.getString(MaxBody);
        if (!s) return nonstd::make_unexpected(s.error());
        body = nlohmann::json::parse(s.value(), nullptr, false);
        return {};
    }

    /// Same protocol version and a JSON object body.
    [[nodiscard]] bool valid() const { return version == kProtocolVersion && body.is_object(); }
};

// client -> server
class HelloPacket : public JsonPacket<HelloPacket, kMaxClientBody> {
    using JsonPacket::JsonPacket;
};
class ScreenActionPacket : public JsonPacket<ScreenActionPacket, kMaxClientBody> {
    using JsonPacket::JsonPacket;
};
class ClientMessagePacket : public JsonPacket<ClientMessagePacket, kMaxClientBody> {
    using JsonPacket::JsonPacket;
};

// server -> client
class WelcomePacket : public JsonPacket<WelcomePacket> {
    using JsonPacket::JsonPacket;
};
class ScreenOpenPacket : public JsonPacket<ScreenOpenPacket> {
    using JsonPacket::JsonPacket;
};
class ScreenUpdatePacket : public JsonPacket<ScreenUpdatePacket> {
    using JsonPacket::JsonPacket;
};
class ScreenClosePacket : public JsonPacket<ScreenClosePacket> {
    using JsonPacket::JsonPacket;
};
class ServerMessagePacket : public JsonPacket<ServerMessagePacket> {
    using JsonPacket::JsonPacket;
};

/// Mod version string sent in the handshake.
[[nodiscard]] std::string modVersion();

} // namespace bs::net
