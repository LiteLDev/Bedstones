// U9: Waystones' edit permissions, visibility rules and the global / team indexes.

#include "feature/access/Access.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/player/PlayerJoinEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/platform/UUID.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include "feature/access/GeneralConfig.h"
#include "game/Waystones.h"
#include "game/WorldDb.h"
#include "mod/Config.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "service/Services.h"

BS_REGISTER_CONFIG("general", bs::feature::access::GeneralConfig);

namespace bs::feature::access {

namespace {

using core::Visibility;
using core::WaystoneRecord;

constexpr std::string_view kTeamsKey = "bs:access:teams";

bool equalsIgnoreCase(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}

GeneralConfig const& cfg() { return config::get<GeneralConfig>(); }

/// In-memory state of the current world.
struct State {
    bool                                         installed = false;
    std::set<std::string>                        global;      ///< uids listed for everyone
    std::map<std::string, std::set<std::string>> teamByOwner; ///< owner uuid -> TEAM waystone uids
    std::map<std::string, std::string>           teamOwner;   ///< TEAM waystone uid -> owner uuid
    std::map<std::string, std::vector<std::string>> teams;    ///< player uuid -> team names (cache)
    bool                                         teamsLoaded = false;
    bool                                         teamsDirty  = false;
    int                                          listener    = -1;
};
State& state() {
    static State s;
    return s;
}

void unindex(std::string const& uid) {
    auto& s = state();
    s.global.erase(uid);
    if (auto it = s.teamOwner.find(uid); it != s.teamOwner.end()) {
        if (auto owned = s.teamByOwner.find(it->second); owned != s.teamByOwner.end()) {
            owned->second.erase(uid);
            if (owned->second.empty()) s.teamByOwner.erase(owned);
        }
        s.teamOwner.erase(it);
    }
}

/// Listed for players who neither own nor activated it (global / team lists).
bool listable(WaystoneRecord const& r) { return !r.transient && !isGeneratedUnnamed(r); }

void index(WaystoneRecord const& r) {
    auto& s = state();
    unindex(r.uid);
    if (!listable(r)) return;
    if (r.visibility == Visibility::Global) {
        s.global.insert(r.uid);
    } else if (r.visibility == Visibility::Team && r.hasOwner()) {
        s.teamByOwner[r.ownerUid].insert(r.uid);
        s.teamOwner[r.uid] = r.ownerUid;
    }
}

void clearIndexes() {
    auto& s = state();
    s.global.clear();
    s.teamByOwner.clear();
    s.teamOwner.clear();
}

void ensureTeamsLoaded() {
    auto& s = state();
    if (s.teamsLoaded || !game::db::available()) return;
    s.teamsLoaded = true;
    s.teams.clear();
    if (auto doc = game::db::load(std::string(kTeamsKey)); doc && doc->is_object()) {
        for (auto const& [uuid, list] : doc->items()) {
            if (!list.is_array()) continue;
            auto& out = s.teams[uuid];
            for (auto const& t : list) {
                if (t.is_string()) out.push_back(t.get<std::string>());
            }
        }
    }
}

void saveTeams() {
    auto& s = state();
    if (!s.teamsDirty || !game::db::available()) return;
    nlohmann::json doc = nlohmann::json::object();
    for (auto const& [uuid, list] : s.teams) doc[uuid] = list;
    game::db::save(std::string(kTeamsKey), doc);
    s.teamsDirty = false;
}

std::vector<std::string> tagsToTeams(Player const& player) {
    std::vector<std::string> out;
    for (auto const& tag : player.getTags()) {
        if (tag.size() > kTeamTagPrefix.size() && tag.starts_with(kTeamTagPrefix)) {
            out.push_back(tag.substr(kTeamTagPrefix.size()));
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

Player* onlinePlayer(std::string const& uuid) {
    auto level = ll::service::getLevel();
    if (!level || uuid.empty()) return nullptr;
    return level->getPlayer(mce::UUID::fromString(uuid));
}

void refreshOnlineTeams() {
    auto level = ll::service::getLevel();
    if (!level) return;
    level->forEachPlayer([](Player& p) {
        (void)teamsOf(p);
        return true;
    });
}

bool intersects(std::vector<std::string> const& a, std::vector<std::string> const& b) {
    for (auto const& x : a) {
        if (std::find(b.begin(), b.end(), x) != b.end()) return true;
    }
    return false;
}

bool isOwner(Player const& player, WaystoneRecord const& r) {
    return r.hasOwner() && r.ownerUid == game::Waystones::playerKey(player);
}

/// The `waystones:edit_all` permission: creative players.
bool skipsPermissions(Player const& player) { return player.isCreative(); }

// ---- the seam ------------------------------------------------------------------------------------

class Permissions final : public service::IPermissions {
public:
    std::optional<Denied> canEdit(Player const& player, WaystoneRecord const& r) override {
        if (skipsPermissions(player)) return std::nullopt;
        if (r.hasOwner() && isRestricted(core::Origin::Player) && !isOwner(player, r)) {
            return Denied{"chat.waystones.only_owner_can_edit", "Only the owner may edit this waystone."};
        }
        if (r.origin != core::Origin::Player && isRestricted(r.origin)) {
            return Denied{"chat.waystones.only_creative_can_edit", "Only creative users may edit this waystone."};
        }
        if (r.isGlobal() && !canManageGlobal(player)) {
            return Denied{"chat.waystones.only_creative_can_edit_global", "Only creative users may edit this waystone."};
        }
        return std::nullopt;
    }

    bool canManageGlobal(Player const& player) override {
        if (skipsPermissions(player) || cfg().allowEveryoneToManageGlobalWaystones) return true;
        if (configuredDefaultVisibility() == Visibility::Global) return true;
        auto const& allowed = cfg().allowedVisibilities;
        return std::any_of(allowed.begin(), allowed.end(), [](auto const& v) { return equalsIgnoreCase(v, "GLOBAL"); });
    }

    std::vector<Visibility> visibilityOptions(Player const& player, WaystoneRecord const& r) override {
        if (r.isSharestone()) return {Visibility::Sharestone};
        if (r.type == core::types::kWarpPlate) return {Visibility::ShardOnly};
        if (!r.isWaystone()) return {r.visibility};
        std::vector<Visibility> out;
        auto add = [&out](Visibility v) {
            if (std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
        };
        add(configuredDefaultVisibility());
        add(Visibility::Activation);
        add(Visibility::Team);
        if (canManageGlobal(player)) add(Visibility::Global);
        return out;
    }

    Visibility defaultVisibility() override { return configuredDefaultVisibility(); }

    bool canSee(Player const& player, WaystoneRecord const& r) override {
        switch (r.visibility) {
        case Visibility::Sharestone:
            return !r.transient;
        case Visibility::ShardOnly:
            return false;
        case Visibility::Activation:
            return isOwner(player, r) || game::Waystones::get().isActivated(player, r.uid);
        case Visibility::Global:
            if (isOwner(player, r) || game::Waystones::get().isActivated(player, r.uid)) return true;
            return listable(r);
        case Visibility::Team:
            if (isOwner(player, r) || game::Waystones::get().isActivated(player, r.uid)) return true;
            return listable(r) && r.hasOwner() && sameTeam(player, r.ownerUid);
        }
        return false;
    }
};

// ---- module ---------------------------------------------------------------------------------------

class AccessModule {
public:
    void install() {
        auto& s = state();
        if (!s.installed) {
            s.installed = true;
            static bool registered = false;
            if (!registered) {
                // WorldDb has no listener removal: register once, act only while installed.
                registered = true;
                game::db::onWorldReady([] {
                    if (!state().installed) return;
                    state().teamsLoaded = false;
                    rebuildIndexes();
                });
                game::db::onWorldSave([] {
                    if (!state().installed) return;
                    refreshOnlineTeams();
                    saveTeams();
                });
                game::db::onWorldClosed([] {
                    clearIndexes();
                    state().teams.clear();
                    state().teamsLoaded = false;
                    state().teamsDirty  = false;
                });
            }
        }
        s.listener = game::Waystones::get().onChanged([](game::WaystoneChange change, WaystoneRecord const& r) {
            if (change == game::WaystoneChange::Removed) unindex(r.uid);
            else index(r);
        });
        // Keep offline owners' teams current: record the tags of players joining and leaving
        // (plus every online player on autosave, for tags changed mid-session).
        auto& bus = ll::event::EventBus::getInstance();
        mEvents.push_back(bus.emplaceListener<ll::event::PlayerJoinEvent>([](ll::event::PlayerJoinEvent& e) {
            (void)teamsOf(static_cast<Player&>(e.self()));
        }));
        mEvents.push_back(bus.emplaceListener<ll::event::PlayerDisconnectEvent>([](ll::event::PlayerDisconnectEvent& e) {
            (void)teamsOf(static_cast<Player&>(e.self()));
        }));
        service::set<service::IPermissions>(std::make_unique<Permissions>());
        if (game::db::available()) rebuildIndexes();
    }

    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mEvents) bus.removeListener(l);
        mEvents.clear();
        auto& s = state();
        if (s.listener >= 0) game::Waystones::get().removeListener(s.listener);
        s.listener = -1;
        saveTeams();
        service::reset<service::IPermissions>();
        clearIndexes();
        s.installed = false;
    }

private:
    std::vector<ll::event::ListenerPtr> mEvents;
};

} // namespace

// ---- helpers ----------------------------------------------------------------------------------------

bool isGeneratedUnnamed(WaystoneRecord const& r) {
    bool generated = r.origin == core::Origin::Wilderness || r.origin == core::Origin::Village || r.origin == core::Origin::Dungeon;
    return generated && r.name.empty();
}

bool isRestricted(core::Origin origin) {
    auto name = core::toString(origin);
    auto const& list = cfg().restrictedWaystones;
    return std::any_of(list.begin(), list.end(), [name](auto const& v) { return equalsIgnoreCase(v, name); });
}

Visibility configuredDefaultVisibility() {
    auto const& v = cfg().defaultVisibility;
    if (equalsIgnoreCase(v, "GLOBAL")) return Visibility::Global;
    if (equalsIgnoreCase(v, "TEAM")) return Visibility::Team;
    return Visibility::Activation;
}

std::vector<std::string> teamsOf(Player const& player) {
    ensureTeamsLoaded();
    auto  teams = tagsToTeams(player);
    auto& s     = state();
    auto  key   = game::Waystones::playerKey(player);
    auto  it    = s.teams.find(key);
    if (teams.empty()) {
        if (it != s.teams.end()) {
            s.teams.erase(it);
            s.teamsDirty = true;
        }
    } else if (it == s.teams.end() || it->second != teams) {
        s.teams[key] = teams;
        s.teamsDirty = true;
    }
    return teams;
}

std::vector<std::string> teamsOf(std::string const& playerUuid) {
    if (auto* p = onlinePlayer(playerUuid)) return teamsOf(*p);
    ensureTeamsLoaded();
    auto& s = state();
    if (auto it = s.teams.find(playerUuid); it != s.teams.end()) return it->second;
    return {};
}

bool sameTeam(Player const& viewer, std::string const& otherUuid) {
    if (otherUuid.empty()) return false;
    auto mine = teamsOf(viewer);
    if (mine.empty()) return false;
    return intersects(mine, teamsOf(otherUuid));
}

std::vector<WaystoneRecord*> globalWaystones() {
    std::vector<WaystoneRecord*> out;
    auto& ws = game::Waystones::get();
    for (auto const& uid : state().global) {
        if (auto* r = ws.find(uid)) out.push_back(r);
    }
    return out;
}

std::vector<WaystoneRecord*> teamWaystones(Player const& viewer) {
    std::vector<WaystoneRecord*> out;
    auto& ws   = game::Waystones::get();
    auto  self = game::Waystones::playerKey(viewer);
    auto  mine = teamsOf(viewer);
    for (auto const& [owner, uids] : state().teamByOwner) {
        if (owner != self && (mine.empty() || !intersects(mine, teamsOf(owner)))) continue;
        for (auto const& uid : uids) {
            if (auto* r = ws.find(uid)) out.push_back(r);
        }
    }
    return out;
}

void rebuildIndexes() {
    clearIndexes();
    if (!game::db::available()) return;
    for (auto* r : game::Waystones::get().all()) index(*r);
    log().info("access: indexes rebuilt ({} global, {} team owners)", state().global.size(), state().teamByOwner.size());
}

} // namespace bs::feature::access

BS_REGISTER_MODULE(bs::feature::access::AccessModule);
