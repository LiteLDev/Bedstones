#include "feature/visuals/VisualsSync.h"

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/server/ServerStoppingEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include "game/Waystones.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "net/Companion.h"
#include "net/Messages.h"

namespace bs::feature::visuals {

namespace {

struct State {
    std::unordered_map<std::int64_t, std::vector<core::visuals::Entry>> lastSent; ///< player unique id -> entries
    std::set<std::string>                         dirtyPlayers; ///< player uuids to re-sync next tick
    bool                                          allDirty = false;
    int                                           ticks    = 0;
};
State gState; // server thread

std::int64_t keyOf(Player const& p) { return p.getOrCreateUniqueID().rawID; }

void forEachCompanion(std::function<void(Player&)> const& fn) {
    auto level = ll::service::getLevel();
    if (!level) return;
    level->forEachPlayer([&](Player& p) {
        if (!p.isClientSide() && net::hasCompanion(p)) fn(p);
        return true;
    });
}

} // namespace

std::vector<core::visuals::Entry> entriesFor(Player& player) {
    auto& ws = game::Waystones::get();
    if (!ws.available()) return {};
    auto  pos  = player.getPosition();
    int   dim  = static_cast<int>(player.getDimensionId());
    auto& data = ws.playerData(player);
    return core::visuals::select(
        ws.all(),
        dim,
        pos.x,
        pos.y,
        pos.z,
        core::visuals::kSyncRange,
        [&data](std::string const& uid) { return data.isActivated(uid); }
    );
}

bool sync(Player& player, bool force) {
    if (!net::hasCompanion(player)) return false;
    auto entries = entriesFor(player);
    auto key     = keyOf(player);
    if (!force) {
        auto it = gState.lastSent.find(key);
        if (it != gState.lastSent.end() && it->second == entries) return false;
    }
    net::sendMessage(player, kChannel, core::visuals::toJson(entries));
    gState.lastSent[key] = entries;
    std::size_t active = 0;
    for (auto const& e : entries) active += e.activated ? 1 : 0;
    log().info("visuals: synced {} waystones ({} activated) to {}", entries.size(), active, player.getRealName());
    return true;
}

class VisualsSyncModule {
public:
    void install() {
        auto& ws = game::Waystones::get();
        mRecordListener = ws.onChanged([](game::WaystoneChange, core::WaystoneRecord const& r) {
            if (core::visuals::isRendered(r.type)) gState.allDirty = true;
        });
        mPlayerListener = ws.onPlayerChanged([](game::PlayerChange change, std::string const& player, std::string const&) {
            if (change != game::PlayerChange::DataUpdated) gState.dirtyPlayers.insert(player);
        });
        auto& bus = ll::event::EventBus::getInstance();
        mListeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([](auto&) { tick(); }));
        mListeners.push_back(bus.emplaceListener<ll::event::PlayerDisconnectEvent>([](ll::event::PlayerDisconnectEvent& ev) {
            gState.lastSent.erase(keyOf(ev.self()));
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::ServerStoppingEvent>([](auto&) { gState = {}; }));
    }
    void uninstall() {
        auto& ws = game::Waystones::get();
        ws.removeListener(mRecordListener);
        ws.removeListener(mPlayerListener);
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
        gState = {};
    }

private:
    static void tick() {
        bool periodic = ++gState.ticks >= kRefreshTicks;
        if (periodic) gState.ticks = 0;
        if (!periodic && !gState.allDirty && gState.dirtyPlayers.empty()) return;
        bool all   = periodic || gState.allDirty;
        auto dirty = std::move(gState.dirtyPlayers);
        gState.dirtyPlayers.clear();
        gState.allDirty = false;
        forEachCompanion([&](Player& p) {
            if (all || dirty.contains(game::Waystones::playerKey(p))) sync(p);
        });
    }

    int                                 mRecordListener = -1;
    int                                 mPlayerListener = -1;
    std::vector<ll::event::ListenerPtr> mListeners;
};

} // namespace bs::feature::visuals

BS_REGISTER_MODULE(bs::feature::visuals::VisualsSyncModule);
