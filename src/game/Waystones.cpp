#include "game/Waystones.h"

#include <algorithm>
#include <exception>
#include <memory>
#include <set>
#include <unordered_map>

#include "mc/platform/UUID.h"
#include "mc/world/actor/player/Player.h"

#include "game/WorldDb.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace bs::game {

namespace {
constexpr std::string_view kRecordsKey   = "bs:waystones";
constexpr std::string_view kPlayerPrefix = "bs:player:";
constexpr int              kVersion      = 1;
} // namespace

struct Waystones::Impl {
    bool loaded = false;
    bool dirty  = false;
    std::vector<std::unique_ptr<core::WaystoneRecord>>         records; // creation order
    std::unordered_map<std::string, core::WaystoneRecord*>     byUid;
    std::unordered_map<std::string, core::WaystoneRecord*>     byPos; // GlobalPos::key()
    std::unordered_map<std::string, core::PlayerWaystoneData>  players;
    std::set<std::string>                                      dirtyPlayers;
    std::vector<std::pair<int, RecordListener>>                listeners;
    std::vector<std::pair<int, PlayerListener>>                playerListeners;
    int                                                        nextListener = 1;

    void index() {
        byUid.clear();
        byPos.clear();
        for (auto& r : records) {
            byUid[r->uid]         = r.get();
            byPos[r->pos.key()] = r.get();
        }
    }
};

Waystones::Waystones() : mImpl(new Impl) {}

Waystones& Waystones::get() {
    static Waystones instance;
    return instance;
}

bool Waystones::available() const { return db::available(); }

void Waystones::ensureLoaded() {
    if (mImpl->loaded || !db::available()) return;
    mImpl->loaded = true;
    mImpl->records.clear();
    if (auto doc = db::load(std::string(kRecordsKey))) {
        if (auto it = doc->find("records"); it != doc->end() && it->is_array()) {
            for (auto const& j : *it) {
                auto r = core::WaystoneRecord::fromJson(j);
                if (r.uid.empty()) continue;
                mImpl->records.push_back(std::make_unique<core::WaystoneRecord>(std::move(r)));
            }
        }
    }
    mImpl->index();
    log().debug("waystones: {} records loaded", mImpl->records.size());
}

core::WaystoneRecord* Waystones::create(core::WaystoneRecord record) {
    ensureLoaded();
    if (!mImpl->loaded) return nullptr;
    if (record.uid.empty()) record.uid = core::randomUuid();
    if (auto it = mImpl->byUid.find(record.uid); it != mImpl->byUid.end()) {
        // Same uid again (a silk-touched waystone placed back): replace in place.
        mImpl->byPos.erase(it->second->pos.key());
        *it->second = std::move(record);
        mImpl->byPos[it->second->pos.key()] = it->second;
        mImpl->dirty                        = true;
        notify(WaystoneChange::Updated, *it->second);
        return it->second;
    }
    auto  owned = std::make_unique<core::WaystoneRecord>(std::move(record));
    auto* ptr   = owned.get();
    mImpl->records.push_back(std::move(owned));
    mImpl->byUid[ptr->uid]       = ptr;
    mImpl->byPos[ptr->pos.key()] = ptr;
    mImpl->dirty                 = true;
    notify(WaystoneChange::Created, *ptr);
    return ptr;
}

core::WaystoneRecord* Waystones::find(std::string_view uid) {
    ensureLoaded();
    auto it = mImpl->byUid.find(std::string(uid));
    return it == mImpl->byUid.end() ? nullptr : it->second;
}

core::WaystoneRecord* Waystones::findAt(core::GlobalPos const& pos) {
    ensureLoaded();
    auto it = mImpl->byPos.find(pos.key());
    return it == mImpl->byPos.end() ? nullptr : it->second;
}

std::vector<core::WaystoneRecord*> Waystones::all(std::function<bool(core::WaystoneRecord const&)> const& pred) {
    ensureLoaded();
    std::vector<core::WaystoneRecord*> out;
    for (auto& r : mImpl->records) {
        if (!pred || pred(*r)) out.push_back(r.get());
    }
    return out;
}

std::vector<core::WaystoneRecord*> Waystones::ofType(std::string_view type) {
    return all([type](core::WaystoneRecord const& r) { return r.type == type; });
}

void Waystones::move(std::string_view uid, core::GlobalPos const& pos) {
    auto* r = find(uid);
    if (!r) return;
    mImpl->byPos.erase(r->pos.key());
    r->pos                     = pos;
    mImpl->byPos[pos.key()]    = r;
    markDirty(uid);
}

void Waystones::markDirty(std::string_view uid) {
    auto* r = find(uid);
    if (!r) return;
    mImpl->dirty = true;
    notify(WaystoneChange::Updated, *r);
}

void Waystones::remove(std::string_view uid) {
    ensureLoaded();
    auto it = mImpl->byUid.find(std::string(uid));
    if (it == mImpl->byUid.end()) return;
    notify(WaystoneChange::Removed, *it->second);
    auto* ptr = it->second;
    mImpl->byPos.erase(ptr->pos.key());
    mImpl->byUid.erase(it);
    std::erase_if(mImpl->records, [ptr](auto const& r) { return r.get() == ptr; });
    mImpl->dirty = true;
}

std::string Waystones::playerKey(Player const& player) { return player.getUuid().asString(); }

core::PlayerWaystoneData& Waystones::playerData(Player const& player) { return playerData(playerKey(player)); }

core::PlayerWaystoneData& Waystones::playerData(std::string const& playerUuid) {
    ensureLoaded();
    auto [it, fresh] = mImpl->players.try_emplace(playerUuid);
    if (fresh) {
        if (auto doc = db::load(std::string(kPlayerPrefix) + playerUuid)) it->second = core::PlayerWaystoneData::fromJson(*doc);
    }
    // Drop waystones that no longer exist (removed while the player was away).
    auto& d       = it->second;
    auto  unknown = [this](std::string const& uid) { return !mImpl->byUid.contains(uid); };
    auto  before  = d.activated.size() + d.sortingIndex.size();
    std::erase_if(d.activated, unknown);
    std::erase_if(d.sortingIndex, unknown);
    if (d.activated.size() + d.sortingIndex.size() != before) mImpl->dirtyPlayers.insert(playerUuid);
    return d;
}

void Waystones::markPlayerDirty(std::string const& playerUuid) {
    mImpl->dirtyPlayers.insert(playerUuid);
    notifyPlayer(PlayerChange::DataUpdated, playerUuid, {});
}

bool Waystones::isActivated(Player const& player, std::string_view uid) { return playerData(player).isActivated(uid); }

bool Waystones::activate(Player const& player, std::string const& uid) {
    if (!find(uid)) return false;
    auto key = playerKey(player);
    if (!playerData(key).activate(uid)) return false;
    mImpl->dirtyPlayers.insert(key);
    notifyPlayer(PlayerChange::Activated, key, uid);
    return true;
}

bool Waystones::forget(Player const& player, std::string const& uid) {
    auto key = playerKey(player);
    if (!playerData(key).forget(uid)) return false;
    mImpl->dirtyPlayers.insert(key);
    notifyPlayer(PlayerChange::Forgotten, key, uid);
    return true;
}

std::vector<std::string> Waystones::knownPlayers() {
    ensureLoaded();
    std::set<std::string> keys;
    for (auto const& [k, _] : mImpl->players) keys.insert(k);
    db::forEachKey(std::string(kPlayerPrefix), [&](std::string_view suffix) {
        if (!suffix.empty()) keys.emplace(suffix);
    });
    return {keys.begin(), keys.end()};
}

int Waystones::onChanged(RecordListener fn) {
    int id = mImpl->nextListener++;
    mImpl->listeners.emplace_back(id, std::move(fn));
    return id;
}

int Waystones::onPlayerChanged(PlayerListener fn) {
    int id = mImpl->nextListener++;
    mImpl->playerListeners.emplace_back(id, std::move(fn));
    return id;
}

void Waystones::removeListener(int id) {
    std::erase_if(mImpl->listeners, [id](auto const& e) { return e.first == id; });
    std::erase_if(mImpl->playerListeners, [id](auto const& e) { return e.first == id; });
}

void Waystones::notify(WaystoneChange change, core::WaystoneRecord const& r) {
    auto copy = mImpl->listeners; // listeners may add / remove listeners
    for (auto const& [_, fn] : copy) {
        try {
            fn(change, r);
        } catch (std::exception const& e) {
            log().error("waystone listener failed: {}", e.what());
        }
    }
}

void Waystones::notifyPlayer(PlayerChange change, std::string const& player, std::string const& uid) {
    auto copy = mImpl->playerListeners;
    for (auto const& [_, fn] : copy) {
        try {
            fn(change, player, uid);
        } catch (std::exception const& e) {
            log().error("player waystone listener failed: {}", e.what());
        }
    }
}

void Waystones::saveDirty() {
    if (!mImpl->loaded || !db::available()) return;
    if (mImpl->dirty) {
        nlohmann::json arr = nlohmann::json::array();
        for (auto const& r : mImpl->records) arr.push_back(r->toJson());
        db::save(std::string(kRecordsKey), {{"version", kVersion}, {"records", arr}});
        mImpl->dirty = false;
    }
    for (auto const& key : mImpl->dirtyPlayers) {
        if (auto it = mImpl->players.find(key); it != mImpl->players.end()) {
            db::save(std::string(kPlayerPrefix) + key, it->second.toJson());
        }
    }
    mImpl->dirtyPlayers.clear();
}

void Waystones::reset() {
    mImpl->loaded = false;
    mImpl->dirty  = false;
    mImpl->records.clear();
    mImpl->byUid.clear();
    mImpl->byPos.clear();
    mImpl->players.clear();
    mImpl->dirtyPlayers.clear();
}

class WaystonesModule {
public:
    void install() {
        static bool registered = false;
        if (registered) return;
        registered = true;
        db::onWorldReady([] { Waystones::get().reset(); });
        db::onWorldSave([] { Waystones::get().saveDirty(); });
        db::onWorldClosed([] { Waystones::get().reset(); });
    }
    void uninstall() { Waystones::get().saveDirty(); }
};

} // namespace bs::game

BS_REGISTER_MODULE_ORDERED(bs::game::WaystonesModule, bs::ModuleOrder::Storage - 10);
