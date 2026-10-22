// Personal Settings screen ("personal_settings", docs/SPEC.md §4.3): per-player alias, user group
// and favorite of one manage entry, plus the links to Edit Waystone and Manage Groups.

#include <algorithm>

#include "ll/api/form/CustomForm.h"

#include "mc/world/actor/player/Player.h"

#include "feature/manage/Manage.h"
#include "game/Waystones.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "net/Screens.h"
#include "service/ScreenOpeners.h"
#include "service/Services.h"

namespace bs::feature::manage {

namespace {

using net::screens::ScreenSession;

std::vector<core::WaystoneGroup> userGroups(Player& player) {
    auto groups = service::get<service::IGroups>().groups(player);
    std::erase_if(groups, [](auto const& g) { return g.inbuilt; });
    return groups;
}

std::vector<std::string> userGroupIds(std::vector<core::WaystoneGroup> const& groups) {
    std::vector<std::string> ids;
    for (auto const& g : groups) ids.push_back(g.id);
    return ids;
}

std::optional<Entry> findListed(Player& player, std::string const& uid, core::WarpSource const& source) {
    if (uid.empty()) return std::nullopt;
    for (auto& e : entries(player, source)) {
        if (e.target.id == uid) return std::move(e);
    }
    return std::nullopt;
}

/// The Edit Waystone button: a waystone record (not a feather target) within 4 blocks.
bool canOpenEdit(Player& player, Entry const& e) {
    if (e.target.kind == "player") return false;
    auto* r = game::Waystones::get().find(e.target.id);
    return r && withinEditRange(player, r->pos);
}

nlohmann::json buildModel(Player& player, Entry const& e, core::WarpSource const& source) {
    auto& data    = game::Waystones::get().playerData(player);
    auto  groups  = userGroups(player);
    auto  ids     = userGroupIds(groups);
    auto* record  = game::Waystones::get().find(e.target.id);
    auto  name    = record ? record->name : e.target.name;
    std::vector<std::string> assigned;
    if (auto it = data.groups.find(e.target.id); it != data.groups.end()) assigned = it->second;
    auto groupsJson = nlohmann::json::array();
    for (auto const& g : groups) groupsJson.push_back({{"id", g.id}, {"name", g.name}, {"icon", g.icon}, {"color", g.color}});
    auto alias = data.aliases.find(e.target.id);
    return {
        {"uid",             e.target.id                                                                      },
        {"name",            name                                                                             },
        {"alias",           alias == data.aliases.end() ? std::string{} : alias->second                      },
        {"canEdit",         canOpenEdit(player, e)                                                           },
        {"hasManageGroups", service::hasScreen("manage_groups")                                               },
        {"groups",          std::move(groupsJson)                                                            },
        {"group",           core::manage::userGroupOf(assigned, ids)                                         },
        {"favorite",        std::find(assigned.begin(), assigned.end(), service::groups::kFavorites) != assigned.end()},
        {"source",          source.toJson()                                                                  },
    };
}

/// Back to Personal Settings after a refused link, or to Manage when the entry is gone.
void reopen(Player& player, std::string const& uid, core::WarpSource const& source) {
    if (!openPersonal(player, uid, source)) openManage(player, source);
}

void handleAction(Player& player, ScreenSession& session, std::string const& action, nlohmann::json const& args) {
    if (action == "close") return;
    auto source = core::WarpSource::fromJson(session.context.value("source", nlohmann::json::object()));
    auto uid    = session.context.value("uid", std::string{});
    if (action == "back") return openManage(player, source);
    if (action != "save" && action != "edit" && action != "manage_groups") return;

    savePersonal(player, uid, source, args.value("alias", std::string{}), args.value("group", std::string{}), args.value("favorite", false));
    if (action == "save") return openManage(player, source);
    if (action == "manage_groups") {
        if (!service::openScreen(player, "manage_groups", {{"source", source.toJson()}, {"from", kPersonalScreen}, {"uid", uid}})) {
            reopen(player, uid, source);
        }
        return;
    }
    // edit: re-check the distance rule before handing over to the edit screen.
    auto e = findListed(player, uid, source);
    if (!e || !canOpenEdit(player, *e)) {
        log().info("personal settings: edit of {} refused for {}", uid, player.getRealName());
        reopen(player, uid, source);
        return;
    }
    log().info("personal settings: edit waystone '{}' by {}", e->target.name, player.getRealName());
    if (service::hasScreen("edit")) {
        service::openScreen(player, "edit", {{"uid", uid}});
    } else {
        net::screens::close(player);
        service::get<service::IEditScreen>().open(player, uid);
    }
}

void showFallback(Player& player, ScreenSession& session) {
    auto const& m   = session.model;
    auto        sid = session.session;
    ll::form::CustomForm form(trFor(player, "gui.waystones.manage_waystones.personal_settings", "Personal Settings"));
    auto name = m.value("name", std::string{});
    form.appendLabel("§e" + (name.empty() ? std::string{"<unnamed>"} : name));
    form.appendInput(
        "alias",
        trFor(player, "bs.manage.form.alias", "Alias"),
        trFor(player, "gui.waystones.personal_waystone_settings.no_alias", "No alias"),
        m.value("alias", std::string{})
    );
    std::vector<std::string> groupIds{""};
    std::vector<std::string> groupLabels{trFor(player, "gui.waystones.personal_waystone_settings.no_group", "No Group")};
    std::size_t              current = 0;
    for (auto const& g : m["groups"]) {
        groupIds.push_back(g.value("id", std::string{}));
        groupLabels.push_back(g.value("name", std::string{}));
        if (groupIds.back() == m.value("group", std::string{})) current = groupIds.size() - 1;
    }
    bool hasGroups = groupIds.size() > 1;
    if (hasGroups) form.appendDropdown("group", trFor(player, "bs.manage.form.group", "Group"), groupLabels, current);
    else form.appendLabel("§7" + trFor(player, "gui.waystones.personal_waystone_settings.no_groups_defined", "No groups defined"));
    form.appendToggle("favorite", trFor(player, "gui.waystones.personal_waystone_settings.favorite", "Favorite"), m.value("favorite", false));
    std::vector<std::string> then{"save"};
    std::vector<std::string> thenLabels{trFor(player, "bs.manage.form.then_back", "Back to Manage Waystones")};
    if (m.value("canEdit", false)) {
        then.emplace_back("edit");
        thenLabels.push_back(trFor(player, "gui.waystones.personal_waystone_settings.edit_waystone", "Edit Waystone"));
    }
    if (m.value("hasManageGroups", false)) {
        then.emplace_back("manage_groups");
        thenLabels.push_back(trFor(player, "gui.waystones.manage_waystones.manage_groups", "Manage Groups"));
    }
    if (then.size() > 1) form.appendDropdown("then", trFor(player, "bs.manage.form.then", "After saving"), thenLabels, 0);
    form.setSubmitButton(trFor(player, "gui.waystones.personal_waystone_settings.save", "Save"));
    form.sendTo(player, [sid, groupIds, then](Player& p, ll::form::CustomFormResult const& res, ll::form::FormCancelReason) {
        if (!net::screens::current(p, kPersonalScreen, sid)) return;
        if (!res) return net::screens::simulateAction(p, "back");
        auto str = [&](char const* key) -> std::string {
            auto it = res->find(key);
            if (it == res->end()) return {};
            auto const* s = std::get_if<std::string>(&it->second);
            return s ? *s : std::string{};
        };
        auto num = [&](char const* key) -> std::uint64_t {
            auto it = res->find(key);
            if (it == res->end()) return 0;
            auto const* v = std::get_if<uint64>(&it->second);
            return v ? *v : 0;
        };
        auto groupIdx = num("group");
        auto thenIdx  = num("then");
        nlohmann::json args{
            {"alias",    str("alias")                                              },
            {"group",    groupIdx < groupIds.size() ? groupIds[groupIdx] : ""},
            {"favorite", num("favorite") != 0                                      },
        };
        net::screens::simulateAction(p, thenIdx < then.size() ? then[thenIdx] : "save", args);
    });
}

} // namespace

bool savePersonal(
    Player&                 player,
    std::string const&      uid,
    core::WarpSource const& source,
    std::string const&      alias,
    std::string const&      requestedGroup,
    bool                    favorite
) {
    auto e = findListed(player, uid, source);
    if (!e) {
        log().info("personal settings: save refused for {} ({} is not listed)", player.getRealName(), uid);
        return false;
    }
    auto  ids   = userGroupIds(userGroups(player));
    auto& ws    = game::Waystones::get();
    auto& data  = ws.playerData(player);
    auto  group = requestedGroup;
    if (!group.empty() && std::find(ids.begin(), ids.end(), group) == ids.end()) {
        // Unknown (deleted / forged) group: keep the current one, still save the rest.
        log().info("personal settings: unknown group {} ignored for {}", group, player.getRealName());
        auto it = data.groups.find(uid);
        group   = it == data.groups.end() ? std::string{} : core::manage::userGroupOf(it->second, ids);
    }
    auto  clean = core::manage::normalizeAlias(alias);
    if (clean) data.aliases[uid] = *clean;
    else data.aliases.erase(uid);
    auto& assigned = data.groups[uid];
    core::manage::setUserGroup(assigned, ids, group);
    core::manage::setMember(assigned, std::string(service::groups::kFavorites), favorite);
    if (assigned.empty()) data.groups.erase(uid);
    ws.markPlayerDirty(game::Waystones::playerKey(player));
    log().info(
        "personal settings: saved '{}' by {} (alias '{}', group '{}', favorite {})",
        e->target.name,
        player.getRealName(),
        clean.value_or(""),
        group,
        favorite
    );
    return true;
}

bool openPersonal(Player& player, std::string const& uid, core::WarpSource const& source) {
    auto e = findListed(player, uid, source);
    if (!e) {
        log().info("personal settings: {} is not listed for {}", uid, player.getRealName());
        return false;
    }
    if (!net::screens::open(player, kPersonalScreen, buildModel(player, *e, source), {{"source", source.toJson()}, {"uid", uid}})) {
        return false;
    }
    log().info("personal settings: opened '{}' for {}", e->target.name, player.getRealName());
    return true;
}

} // namespace bs::feature::manage

BS_REGISTER_SCREEN_ACTIONS(bs::feature::manage::kPersonalScreen, bs::feature::manage::handleAction);
BS_REGISTER_SCREEN_FALLBACK(bs::feature::manage::kPersonalScreen, bs::feature::manage::showFallback);

BS_REGISTER_SCREEN_OPENER("personal_settings", [](Player& player, nlohmann::json const& args) {
    bs::feature::manage::openPersonal(
        player,
        args.value("uid", std::string{}),
        bs::core::WarpSource::fromJson(args.value("source", nlohmann::json::object()))
    );
});
