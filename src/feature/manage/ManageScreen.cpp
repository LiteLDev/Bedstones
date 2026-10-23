// Manage Waystones screen ("manage", docs/SPEC.md §4.2): manual reordering, delete / forget,
// hide / show, links to Personal Settings and Manage Groups. Server side + form fallback.

#include <algorithm>
#include <cmath>

#include "ll/api/form/ModalForm.h"
#include "ll/api/form/SimpleForm.h"
#include "ll/api/service/Bedrock.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"

#include "feature/manage/Manage.h"
#include "game/Blocks.h"
#include "game/Waystones.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "net/Screens.h"
#include "service/ScreenOpeners.h"
#include "service/Services.h"

namespace bs::feature::manage {

namespace {

using core::manage::DeleteAction;
using net::screens::ScreenSession;

bool excluded(core::WarpTarget const& t) {
    return t.pinned || t.kind == core::types::kWarpPortal || t.kind == core::types::kFleetingMemorial;
}

/// Hidden records a selection implementation may already leave out of its list: they stay
/// manageable so they can be shown again.
bool belongsToSource(core::WaystoneRecord const& r, core::WarpSource const& source) {
    auto& ws = game::Waystones::get();
    if (source.kind == core::WarpSourceKind::Sharestone) {
        auto* from = ws.find(source.fromUid);
        return from && from->type == r.type;
    }
    if (source.kind == core::WarpSourceKind::Portstone && source.portstoneColor != "white") {
        return r.type == core::types::sharestone(source.portstoneColor);
    }
    return r.isWaystone();
}

std::string displayName(Entry const& e) { return e.target.name.empty() ? "<unnamed>" : e.target.name; }

Entry const* findEntry(std::vector<Entry> const& list, std::string const& id) {
    auto it = std::find_if(list.begin(), list.end(), [&](Entry const& e) { return e.target.id == id; });
    return it == list.end() ? nullptr : &*it;
}

std::vector<std::string> idsOf(std::vector<Entry> const& list) {
    std::vector<std::string> ids;
    ids.reserve(list.size());
    for (auto const& e : list) ids.push_back(e.target.id);
    return ids;
}

core::WarpSource sourceOf(ScreenSession const& s) {
    return core::WarpSource::fromJson(s.context.value("source", nlohmann::json::object()));
}

nlohmann::json buildModel(Player& player, core::WarpSource const& source) {
    auto rows  = nlohmann::json::array();
    auto actor = actorFacts(player);
    for (auto const& e : entries(player, source)) {
        rows.push_back({
            {"id",        e.target.id                                 },
            {"name",      e.target.name                               },
            {"kind",      e.target.kind                               },
            {"hidden",    e.hidden                                    },
            {"canDelete", core::manage::canDelete(e.facts, actor)     },
            {"canHide",   core::manage::canHide(e.facts, actor)       },
            {"everyone",  core::manage::deleteAffectsEveryone(e.facts)},
            {"current",   e.target.current                            },
        });
    }
    return {
        {"title",           trFor(player, "gui.waystones.manage_waystones.title", "Manage Waystones")},
        {"source",          source.toJson()                                                          },
        {"hasManageGroups", service::hasScreen("manage_groups")                                       },
        {"rows",            std::move(rows)                                                          },
    };
}

void refresh(Player& player, core::WarpSource const& source) {
    if (net::screens::current(player, kManageScreen)) net::screens::update(player, buildModel(player, source));
}

char const* actionName(DeleteAction a) {
    switch (a) {
    case DeleteAction::Forget:
        return "forget";
    case DeleteAction::Purge:
        return "purge";
    case DeleteAction::Demote:
        return "demote";
    case DeleteAction::None:
        break;
    }
    return "none";
}

/// Applies the delete rules (re-validated). Returns false when not allowed.
bool deleteEntry(Player& player, core::WarpSource const& source, std::string const& id) {
    auto list   = entries(player, source);
    auto const* e = findEntry(list, id);
    if (!e) return false;
    auto action = core::manage::deleteAction(e->facts, actorFacts(player));
    auto name   = displayName(*e);
    auto& ws    = game::Waystones::get();
    switch (action) {
    case DeleteAction::None:
        log().info("manage: delete of '{}' refused for {}", name, player.getRealName());
        return false;
    case DeleteAction::Forget:
        ws.forget(player, id);
        break;
    case DeleteAction::Purge:
        ws.remove(id);
        break;
    case DeleteAction::Demote: {
        auto* r = ws.find(id);
        if (!r) return false;
        r->visibility = core::Visibility::Activation;
        auto pos      = r->pos;
        ws.markDirty(id);
        ws.forget(player, id);
        if (blockExists(pos) == std::optional<bool>{false}) ws.remove(id);
        break;
    }
    }
    log().info("manage: deleted '{}' ({}) by {}", name, actionName(action), player.getRealName());
    // The screen's own source was purged: its list can't be rebuilt any more.
    if (id == source.fromUid && !ws.find(id)) net::screens::close(player);
    return true;
}

bool setHidden(Player& player, core::WarpSource const& source, std::string const& id, bool hidden) {
    auto list     = entries(player, source);
    auto const* e = findEntry(list, id);
    if (!e) return false;
    // Hiding needs the eye toggle; showing is always allowed for a hidden entry.
    if (hidden && !core::manage::canHide(e->facts, actorFacts(player))) return false;
    auto& ws   = game::Waystones::get();
    auto& data = ws.playerData(player);
    if (hidden == data.hidden.contains(id)) return false;
    if (hidden) data.hidden.insert(id);
    else data.hidden.erase(id);
    ws.markPlayerDirty(game::Waystones::playerKey(player));
    log().info("manage: {} '{}' by {}", hidden ? "hid" : "showed", displayName(*e), player.getRealName());
    return true;
}

bool reorder(Player& player, core::WarpSource const& source, std::string const& action, nlohmann::json const& args) {
    auto  list = entries(player, source);
    auto& ws   = game::Waystones::get();
    // Only waystone records can be ordered: player data drops other ids (provider targets).
    auto ids = idsOf(list);
    std::erase_if(ids, [&](std::string const& id) { return ws.find(id) == nullptr; });
    auto& idx = ws.playerData(player).sortingIndex;
    bool  ok   = false;
    if (action == "swap") {
        auto from = args.value("from", std::string{}), to = args.value("to", std::string{});
        ok        = core::manage::swap(idx, ids, from, to);
        if (ok) log().info("manage: swapped '{}' <-> '{}' by {}", displayName(*findEntry(list, from)), displayName(*findEntry(list, to)), player.getRealName());
    } else {
        auto id   = args.value("id", std::string{});
        bool top  = action == "move_top";
        ok        = top ? core::manage::moveToTop(idx, ids, id) : core::manage::moveToBottom(idx, ids, id);
        if (ok) log().info("manage: moved '{}' to the {} by {}", displayName(*findEntry(list, id)), top ? "top" : "bottom", player.getRealName());
    }
    if (ok) ws.markPlayerDirty(game::Waystones::playerKey(player));
    return ok;
}

void goBack(Player& player, core::WarpSource const& source) {
    if (service::openScreen(player, "selection", {{"source", source.toJson()}})) return;
    net::screens::close(player);
    service::get<service::ISelection>().open(player, source);
}

void handleAction(Player& player, ScreenSession& session, std::string const& action, nlohmann::json const& args) {
    auto source = sourceOf(session);
    if (action == "close") return;
    if (action == "back") return goBack(player, source);
    if (action == "manage_groups") {
        service::openScreen(player, "manage_groups", {{"source", source.toJson()}, {"from", kManageScreen}});
        return;
    }
    if (action == "personal_settings") {
        openPersonal(player, args.value("id", std::string{}), source);
        return;
    }
    if (action == "swap" || action == "move_top" || action == "move_bottom") reorder(player, source, action, args);
    else if (action == "delete") deleteEntry(player, source, args.value("id", std::string{}));
    else if (action == "set_hidden") setHidden(player, source, args.value("id", std::string{}), args.value("hidden", false));
    else return;
    // Always answer, also when refused: the model resets the client's optimistic state.
    refresh(player, source);
}

// ---- form fallback ------------------------------------------------------------------------------------

void showRowMenu(Player& player, std::uint32_t session, nlohmann::json const& model, std::size_t index) {
    auto const& rows = model["rows"];
    if (index >= rows.size()) return;
    auto const& row  = rows[index];
    auto        id   = row.value("id", std::string{});
    auto        name = row.value("name", std::string{});
    struct Choice {
        std::string    action;
        nlohmann::json args;
    };
    std::vector<Choice> choices;
    ll::form::SimpleForm form(name.empty() ? "<unnamed>" : name);
    auto add = [&](std::string const& label, std::string action, nlohmann::json args) {
        form.appendButton(label);
        choices.push_back({std::move(action), std::move(args)});
    };
    if (index > 0) add(trFor(player, "bs.manage.form.move_up", "Move up"), "swap", {{"from", id}, {"to", rows[index - 1].value("id", "")}});
    if (index + 1 < rows.size()) add(trFor(player, "bs.manage.form.move_down", "Move down"), "swap", {{"from", id}, {"to", rows[index + 1].value("id", "")}});
    if (index > 0) add(trFor(player, "bs.manage.form.move_top", "Move to top"), "move_top", {{"id", id}});
    if (index + 1 < rows.size()) add(trFor(player, "bs.manage.form.move_bottom", "Move to bottom"), "move_bottom", {{"id", id}});
    add(trFor(player, "gui.waystones.manage_waystones.personal_settings", "Personal Settings"), "personal_settings", {{"id", id}});
    if (row.value("canDelete", false)) add("§c" + trFor(player, "bs.manage.form.delete", "Delete"), "delete", {{"id", id}});
    else if (row.value("canHide", false) || row.value("hidden", false)) {
        bool hidden = row.value("hidden", false);
        add(hidden ? trFor(player, "gui.waystones.manage_waystones.show", "Show Waystone") : trFor(player, "gui.waystones.manage_waystones.hide", "Hide Waystone"),
            "set_hidden",
            {{"id", id}, {"hidden", !hidden}});
    }
    bool everyone = row.value("everyone", false);
    form.sendTo(player, [session, choices, name, everyone](Player& p, int picked, ll::form::FormCancelReason) {
        auto* s = net::screens::current(p, kManageScreen, session);
        if (!s) return;
        if (picked < 0 || picked >= static_cast<int>(choices.size())) {
            net::screens::update(p, s->model); // back to the list
            return;
        }
        auto const& c = choices[static_cast<std::size_t>(picked)];
        if (c.action != "delete") return net::screens::simulateAction(p, c.action, c.args);
        // Delete asks first (the native screen gates it behind Shift).
        std::string content = trFor(p, "gui.waystones.manage_waystones.delete", "Click to delete");
        if (everyone) content += "\n§c" + trFor(p, "gui.waystones.manage_waystones.delete_warning", "This will remove the waystone for everyone.");
        ll::form::ModalForm confirm(name, content, trFor(p, "bs.manage.form.delete", "Delete"), trFor(p, "gui.waystones.manage_waystones.back", "Back"));
        confirm.sendTo(p, [session, args = c.args](Player& pp, ll::form::ModalFormResult result, ll::form::FormCancelReason) {
            auto* s2 = net::screens::current(pp, kManageScreen, session);
            if (!s2) return;
            if (result && *result == ll::form::ModalFormSelectedButton::Upper) net::screens::simulateAction(pp, "delete", args);
            else net::screens::update(pp, s2->model);
        });
    });
}

void showFallback(Player& player, ScreenSession& session) {
    auto const& model = session.model;
    auto        sid   = session.session;
    ll::form::SimpleForm form(model.value("title", std::string{"Manage Waystones"}));
    std::vector<std::string> actions; // per button: "back", "manage_groups" or "row:<index>"
    form.appendButton(trFor(player, "gui.waystones.manage_waystones.back", "Back"));
    actions.emplace_back("back");
    if (model.value("hasManageGroups", false)) {
        form.appendButton(trFor(player, "gui.waystones.manage_waystones.manage_groups", "Manage Groups"));
        actions.emplace_back("manage_groups");
    }
    auto const& rows = model["rows"];
    if (rows.empty()) {
        form.setContent("§c" + trFor(player, "gui.waystones.waystone_selection.no_waystones_activated", "You have not activated any waystones yet."));
    }
    for (std::size_t i = 0; i < rows.size(); ++i) {
        auto name = rows[i].value("name", std::string{});
        if (name.empty()) name = "<unnamed>";
        if (rows[i].value("hidden", false)) name = "§8" + name + " §7(" + trFor(player, "bs.manage.form.hidden", "hidden") + ")";
        form.appendButton(name);
        actions.push_back("row:" + std::to_string(i));
    }
    form.sendTo(player, [sid, actions](Player& p, int picked, ll::form::FormCancelReason) {
        auto* s = net::screens::current(p, kManageScreen, sid);
        if (!s) return;
        if (picked < 0 || picked >= static_cast<int>(actions.size())) return net::screens::simulateAction(p, "close");
        auto const& a = actions[static_cast<std::size_t>(picked)];
        if (a.starts_with("row:")) return showRowMenu(p, sid, s->model, std::stoul(a.substr(4)));
        net::screens::simulateAction(p, a);
    });
}

} // namespace

// ---- shared helpers ----------------------------------------------------------------------------------------

core::manage::ActorFacts actorFacts(Player& player) {
    return {player.isCreative(), service::get<service::IPermissions>().canManageGlobal(player)};
}

std::vector<Entry> entries(Player& player, core::WarpSource const& source) {
    auto& ws   = game::Waystones::get();
    auto& data = ws.playerData(player);
    auto  list = service::get<service::ITargets>().list(player, source);
    std::erase_if(list, excluded);
    for (auto const& uid : data.hidden) {
        if (std::any_of(list.begin(), list.end(), [&](auto const& t) { return t.id == uid; })) continue;
        auto* r = ws.find(uid);
        if (!r || r->transient || !belongsToSource(*r, source)) continue;
        if (!data.isActivated(uid) && !service::get<service::IPermissions>().canSee(player, *r)) continue;
        core::WarpTarget t;
        t.id     = r->uid;
        t.kind   = r->isSharestone() ? "sharestone" : r->type;
        t.name   = data.effectiveName(r->uid, r->name);
        t.pos    = r->pos;
        t.global = r->isGlobal();
        list.push_back(std::move(t));
    }
    std::vector<std::string> ids;
    for (auto const& t : list) ids.push_back(t.id);
    auto order = core::manage::ordered(ids, data.sortingIndex);

    std::vector<Entry> out;
    out.reserve(order.size());
    for (auto const& id : order) {
        auto it = std::find_if(list.begin(), list.end(), [&](auto const& t) { return t.id == id; });
        Entry e;
        e.target = *it;
        if (auto* r = ws.find(id)) {
            e.facts.waystone   = r->isWaystone();
            e.facts.sharestone = r->isSharestone();
            e.facts.global     = r->isGlobal();
            e.facts.team       = r->visibility == core::Visibility::Team;
        }
        e.facts.activated = data.isActivated(id);
        e.hidden          = data.hidden.contains(id);
        out.push_back(std::move(e));
    }
    return out;
}

std::optional<bool> blockExists(core::GlobalPos const& pos) {
    auto level = ll::service::getLevel();
    if (!level) return std::nullopt;
    auto dim = level->getDimension(pos.dim).lock();
    if (!dim) return std::nullopt;
    auto id = game::blockIdAt(dim->getBlockSourceFromMainChunkSource(), game::blockPos(pos));
    if (id.empty()) return std::nullopt; // chunk not loaded: unknown
    return id.starts_with("bedstones:");
}

bool withinEditRange(Player& player, core::GlobalPos const& pos) {
    if (static_cast<int>(player.getDimensionId()) != pos.dim) return false;
    auto  feet = player.getFeetPos();
    float dx = feet.x - (static_cast<float>(pos.x) + 0.5f), dy = feet.y - (static_cast<float>(pos.y) + 0.5f),
          dz = feet.z - (static_cast<float>(pos.z) + 0.5f);
    return dx * dx + dy * dy + dz * dz <= 4.f * 4.f;
}

void openManage(Player& player, core::WarpSource const& source) {
    auto model = buildModel(player, source);
    auto count = model["rows"].size();
    if (!net::screens::open(player, kManageScreen, std::move(model), {{"source", source.toJson()}})) return;
    log().info("manage: opened for {} ({} entries)", player.getRealName(), count);
}

} // namespace bs::feature::manage

BS_REGISTER_SCREEN_ACTIONS(bs::feature::manage::kManageScreen, bs::feature::manage::handleAction);
BS_REGISTER_SCREEN_FALLBACK(bs::feature::manage::kManageScreen, bs::feature::manage::showFallback);

BS_REGISTER_SCREEN_OPENER("manage", [](Player& player, nlohmann::json const& args) {
    bs::feature::manage::openManage(player, bs::core::WarpSource::fromJson(args.value("source", nlohmann::json::object())));
});
