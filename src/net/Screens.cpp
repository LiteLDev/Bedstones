#include "net/Screens.h"

#include <exception>
#include <unordered_map>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/server/ServerStoppingEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/network/NetEventCallback.h"
#include "mc/network/NetworkIdentifier.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include "net/Companion.h"
#include "net/Messages.h"
#include "net/Packets.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace bs::net {

// ---- messages -------------------------------------------------------------------------------------

void sendMessage(Player& player, std::string_view channel, nlohmann::json data) {
    ServerMessagePacket packet{nlohmann::json{{"channel", std::string(channel)}, {"data", std::move(data)}}};
    send(player, packet);
}

void broadcastMessage(std::string_view channel, nlohmann::json const& data) {
    auto level = ll::service::getLevel();
    if (!level) return;
    level->forEachPlayer([&](Player& player) {
        if (!player.isClientSide() && hasCompanion(player)) sendMessage(player, channel, data);
        return true;
    });
}

void simulateClientMessage(Player& player, std::string_view channel, nlohmann::json const& data) {
    for (auto const& h : ClientMessageRegistry::entries()) {
        if (h.channel != channel || !h.handle) continue;
        try {
            h.handle(player, data);
        } catch (std::exception const& e) {
            log().error("client message {} failed: {}", channel, e.what());
        }
    }
}

class ClientMessageHandlerImpl : public ll::network::PacketHandlerBase<ClientMessageHandlerImpl, ClientMessagePacket> {
public:
    void handlePacket(NetworkIdentifier const& source, NetEventCallback& callback, ClientMessagePacket const& packet) const {
        auto* player = senderOf(source, callback, packet.mSenderSubId);
        if (!player || !packet.valid() || !hasCompanion(*player)) return;
        auto channel = packet.body.value("channel", std::string{});
        simulateClientMessage(*player, channel, packet.body.value("data", nlohmann::json{}));
    }
};

// ---- screens --------------------------------------------------------------------------------------

namespace screens {

namespace {
std::unordered_map<std::int64_t, ScreenSession> gOpen; // player unique id -> screen
std::uint32_t                                   gNextSession = 1;

std::int64_t keyOf(Player const& p) { return p.getOrCreateUniqueID().rawID; }

FallbackHandler const* fallbackFor(std::string const& id) {
    for (auto const& h : ScreenHandlerRegistry::entries()) {
        if (h.id == id && h.fallback) return &h.fallback;
    }
    return nullptr;
}

/// `closed`: the session was already removed (the "close" action); handlers get `session` itself.
void dispatch(Player& player, ScreenSession& session, std::string const& action, nlohmann::json const& args, bool closed = false) {
    // Copy: a handler may open another screen (replacing `session`).
    auto id = session.id;
    for (auto const& h : ScreenHandlerRegistry::entries()) {
        if (h.id != id || !h.actions) continue;
        auto* live = closed ? &session : current(player, id, session.session);
        if (!live) return;
        try {
            h.actions(player, *live, action, args);
        } catch (std::exception const& e) {
            log().error("screen {} action {} failed: {}", id, action, e.what());
        }
    }
}
} // namespace

std::uint32_t open(Player& player, std::string const& id, nlohmann::json model, nlohmann::json context) {
    ScreenSession s;
    s.session = gNextSession++;
    s.id      = id;
    s.model   = std::move(model);
    s.context = std::move(context);
    if (hasCompanion(player)) {
        auto& slot = gOpen[keyOf(player)] = std::move(s);
        send(player, ScreenOpenPacket{nlohmann::json{{"session", slot.session}, {"id", slot.id}, {"model", slot.model}}});
        log().debug("screen {} opened for {} (session {})", slot.id, player.getRealName(), slot.session);
        return slot.session;
    }
    auto const* fb = fallbackFor(id);
    if (!fb) {
        log().debug("screen {}: {} has no client mod and the screen has no form fallback", id, player.getRealName());
        return 0;
    }
    s.fallback = true;
    auto& slot = gOpen[keyOf(player)] = std::move(s);
    auto  sid  = slot.session;
    try {
        (*fb)(player, slot);
    } catch (std::exception const& e) {
        log().error("screen {} fallback failed: {}", id, e.what());
    }
    return sid;
}

bool update(Player& player, nlohmann::json model) {
    auto* s = current(player);
    if (!s) return false;
    s->model = std::move(model);
    if (s->fallback) {
        if (auto const* fb = fallbackFor(s->id)) (*fb)(player, *s);
        return true;
    }
    send(player, ScreenUpdatePacket{nlohmann::json{{"session", s->session}, {"model", s->model}}});
    return true;
}

void close(Player& player) {
    auto it = gOpen.find(keyOf(player));
    if (it == gOpen.end()) return;
    auto session = it->second.session;
    bool fb      = it->second.fallback;
    gOpen.erase(it);
    if (!fb) send(player, ScreenClosePacket{nlohmann::json{{"session", session}}});
}

ScreenSession* current(Player& player) {
    auto it = gOpen.find(keyOf(player));
    return it == gOpen.end() ? nullptr : &it->second;
}

ScreenSession* current(Player& player, std::string const& id, std::uint32_t session) {
    auto* s = current(player);
    if (!s || s->id != id || (session && s->session != session)) return nullptr;
    return s;
}

void simulateAction(Player& player, std::string const& action, nlohmann::json const& args) {
    auto* s = current(player);
    if (!s) return;
    if (action == "close") {
        auto copy = *s;
        gOpen.erase(keyOf(player));
        dispatch(player, copy, action, args, true); // handlers see the closed session
        return;
    }
    dispatch(player, *s, action, args);
}

class ScreenActionHandler : public ll::network::PacketHandlerBase<ScreenActionHandler, ScreenActionPacket> {
public:
    void handlePacket(NetworkIdentifier const& source, NetEventCallback& callback, ScreenActionPacket const& packet) const {
        auto* player = senderOf(source, callback, packet.mSenderSubId);
        if (!player || !packet.valid()) return;
        auto* s = current(*player);
        // Actions of a screen that was replaced meanwhile are stale: drop them.
        if (!s || packet.body.value("session", 0u) != s->session) return;
        auto action = packet.body.value("action", std::string{});
        if (action.empty()) return;
        simulateAction(*player, action, packet.body.value("args", nlohmann::json::object()));
    }
};

class ScreensModule {
public:
    void install() {
        auto& bus = ll::event::EventBus::getInstance();
        mListeners.push_back(bus.emplaceListener<ll::event::PlayerDisconnectEvent>([](ll::event::PlayerDisconnectEvent& ev) {
            gOpen.erase(keyOf(ev.self()));
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::ServerStoppingEvent>([](auto&) { gOpen.clear(); }));
    }
    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
        gOpen.clear();
    }

private:
    std::vector<ll::event::ListenerPtr> mListeners;
};

} // namespace screens
} // namespace bs::net

BS_REGISTER_MODULE_ORDERED(bs::net::screens::ScreensModule, bs::ModuleOrder::Net + 1);
