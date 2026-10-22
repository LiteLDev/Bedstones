#ifdef BS_CLIENT

#include "client/net/ClientNet.h"

#include <exception>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/network/NetEventCallback.h"
#include "mc/network/ServerNetworkHandler.h"

#include "client/Client.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "net/Packets.h"

namespace bs::client::net {

namespace {
using bs::net::ClientMessagePacket;
using bs::net::HelloPacket;
using bs::net::ScreenActionPacket;
using bs::net::ScreenClosePacket;
using bs::net::ScreenOpenPacket;
using bs::net::ScreenUpdatePacket;
using bs::net::ServerMessagePacket;
using bs::net::WelcomePacket;

constexpr int kHelloEveryTicks = 40; ///< 2 s
constexpr int kHelloAttempts   = 15;

struct State {
    bool                        ready        = false;
    bool                        incompatible = false;
    int                         helloSent    = 0;
    int                         helloTimer   = 0;
    std::optional<ServerScreen> screen;
    std::uint64_t               screenRev = 0;
};
State gState;

std::vector<std::function<void()>>& leaveListeners() {
    static std::vector<std::function<void()>> v;
    return v;
}

/// True when a server-to-client packet really reached the client (not this process's server).
bool fromServer(NetEventCallback& callback) {
    auto handler = ll::service::getServerNetworkHandler();
    return !handler || static_cast<NetEventCallback*>(handler.as_ptr()) != &callback;
}

void reset() {
    gState = {};
    ++gState.screenRev;
    for (auto const& fn : leaveListeners()) {
        try {
            fn();
        } catch (std::exception const& e) {
            log().error("leave-world listener failed: {}", e.what());
        }
    }
}

void sendHello() {
    HelloPacket hello{nlohmann::json{{"version", bs::net::modVersion()}, {"features", {"native_screen"}}}};
    hello.sendToServer();
    ++gState.helloSent;
    if (gState.helloSent == 1) log().info("companion: hello sent to the server");
}
} // namespace

bool serverReady() { return gState.ready; }

void sendMessage(std::string_view channel, nlohmann::json data) {
    if (!gState.ready) return;
    ClientMessagePacket{nlohmann::json{{"channel", std::string(channel)}, {"data", std::move(data)}}}.sendToServer();
}

ServerScreen const* serverScreen() { return gState.screen ? &*gState.screen : nullptr; }

std::uint64_t screenRevision() { return gState.screenRev; }

void sendScreenAction(std::string const& action, nlohmann::json args) {
    if (!gState.ready || !gState.screen) return;
    ScreenActionPacket{nlohmann::json{{"session", gState.screen->session}, {"action", action}, {"args", std::move(args)}}}
        .sendToServer();
}

void closeServerScreen() {
    if (!gState.screen) return;
    sendScreenAction("close");
    gState.screen.reset();
    ++gState.screenRev;
}

void onLeaveWorld(std::function<void()> fn) { leaveListeners().push_back(std::move(fn)); }

// ---- packet handlers (client thread) --------------------------------------------------------------

class WelcomeHandler : public ll::network::PacketHandlerBase<WelcomeHandler, WelcomePacket> {
public:
    void handlePacket(NetworkIdentifier const&, NetEventCallback& cb, WelcomePacket const& p) const {
        if (!fromServer(cb)) return;
        if (!p.valid() || p.body.value("incompatible", false)) {
            if (!gState.incompatible) {
                log().warn("companion: the server's Bedstones is incompatible (protocol {}); using the form UI", p.version);
            }
            gState.incompatible = true;
            gState.ready        = false;
            return;
        }
        if (!gState.ready) log().info("companion: server ready (server mod {})", p.body.value("version", std::string{}));
        gState.ready = true;
    }
};

class ScreenOpenHandler : public ll::network::PacketHandlerBase<ScreenOpenHandler, ScreenOpenPacket> {
public:
    void handlePacket(NetworkIdentifier const&, NetEventCallback& cb, ScreenOpenPacket const& p) const {
        if (!fromServer(cb) || !p.valid()) return;
        ServerScreen s;
        s.session = p.body.value("session", 0u);
        s.id      = p.body.value("id", std::string{});
        s.model   = p.body.value("model", nlohmann::json::object());
        log().info("companion: screen {} opened (session {})", s.id, s.session);
        gState.screen = std::move(s);
        ++gState.screenRev;
    }
};

class ScreenUpdateHandler : public ll::network::PacketHandlerBase<ScreenUpdateHandler, ScreenUpdatePacket> {
public:
    void handlePacket(NetworkIdentifier const&, NetEventCallback& cb, ScreenUpdatePacket const& p) const {
        if (!fromServer(cb) || !p.valid() || !gState.screen) return;
        if (p.body.value("session", 0u) != gState.screen->session) return;
        gState.screen->model = p.body.value("model", nlohmann::json::object());
        ++gState.screenRev;
    }
};

class ScreenCloseHandler : public ll::network::PacketHandlerBase<ScreenCloseHandler, ScreenClosePacket> {
public:
    void handlePacket(NetworkIdentifier const&, NetEventCallback& cb, ScreenClosePacket const& p) const {
        if (!fromServer(cb) || !p.valid() || !gState.screen) return;
        if (p.body.value("session", 0u) != gState.screen->session) return;
        gState.screen.reset();
        ++gState.screenRev;
    }
};

class ServerMessageHandlerImpl : public ll::network::PacketHandlerBase<ServerMessageHandlerImpl, ServerMessagePacket> {
public:
    void handlePacket(NetworkIdentifier const&, NetEventCallback& cb, ServerMessagePacket const& p) const {
        if (!fromServer(cb) || !p.valid()) return;
        auto channel = p.body.value("channel", std::string{});
        auto data    = p.body.value("data", nlohmann::json{});
        for (auto const& h : ServerMessageRegistry::entries()) {
            if (h.channel != channel || !h.handle) continue;
            try {
                h.handle(data);
            } catch (std::exception const& e) {
                log().error("server message {} failed: {}", channel, e.what());
            }
        }
    }
};

// ---- module -----------------------------------------------------------------------------------------

class ClientNetModule {
public:
    void install() {
        auto& bus = ll::event::EventBus::getInstance();
        mListeners.push_back(bus.emplaceListener<ll::event::ClientLevelTickEvent>([](auto&) {
            if (gState.ready || gState.incompatible || gState.helloSent >= kHelloAttempts) return;
            if (!currentPlayer()) return;
            if (--gState.helloTimer > 0) return;
            gState.helloTimer = kHelloEveryTicks;
            sendHello();
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) { reset(); }));
    }
    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
        reset();
    }

private:
    std::vector<ll::event::ListenerPtr> mListeners;
};

} // namespace bs::client::net

BS_REGISTER_MODULE(bs::client::net::ClientNetModule);

#endif // BS_CLIENT
