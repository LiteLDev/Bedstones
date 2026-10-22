// U12 Groups: server side of the Manage Groups and Edit Group screens (docs/SPEC.md §4.4):
// models, client actions (re-validated here), form fallbacks, screen openers and the
// `/waystones debug groups` subcommand.

#include <algorithm>
#include <array>

#include "ll/api/command/CommandHandle.h"
#include "ll/api/form/CustomForm.h"
#include "ll/api/form/ModalForm.h"
#include "ll/api/form/SimpleForm.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"

#include "core/Warp.h"
#include "feature/groups/Groups.h"
#include "game/Waystones.h"
#include "mod/Commands.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "net/Screens.h"
#include "service/ScreenOpeners.h"
#include "service/Services.h"

namespace bs::feature::groups {

namespace cg = core::groups;
using nlohmann::json;
using net::screens::ScreenSession;

namespace {

std::string playerName(Player const& p) { return p.getRealName(); }

// Type-safe reads of client / opener args (never throw on unexpected types).
std::string strArg(json const& args, char const* key) {
    auto it = args.is_object() ? args.find(key) : args.end();
    return it != args.end() && it->is_string() ? it->get<std::string>() : std::string{};
}
int intArg(json const& args, char const* key, int fallback) {
    auto it = args.is_object() ? args.find(key) : args.end();
    return it != args.end() && it->is_number_integer() ? it->get<int>() : fallback;
}
bool boolArg(json const& args, char const* key, bool fallback) {
    auto it = args.is_object() ? args.find(key) : args.end();
    return it != args.end() && it->is_boolean() ? it->get<bool>() : fallback;
}

void saveData(Player const& player) { game::Waystones::get().markPlayerDirty(game::Waystones::playerKey(player)); }

/// Return args for Manage Groups without the edit-only keys.
json returnArgsOf(json const& args) {
    json out = json::object();
    if (!args.is_object()) return out;
    if (auto it = args.find("source"); it != args.end() && it->is_object()) out["source"] = *it;
    for (auto const* key : {"from", "uid"}) {
        if (auto it = args.find(key); it != args.end() && it->is_string()) out[key] = *it;
    }
    return out;
}

json manageModel(Player& player) {
    json list = json::array();
    for (auto const& g : groupsFor(player)) {
        list.push_back({
            {"id", g.id},
            {"name", g.name},
            {"icon", g.icon},
            {"color", g.color},
            {"inbuilt", g.inbuilt},
            {"hidden", g.hidden},
        });
    }
    return {{"groups", list}};
}

void refreshManage(Player& player) {
    if (net::screens::current(player, kManageScreen)) net::screens::update(player, manageModel(player));
}

/// Back from Manage Groups: the screen it came from, else the selection menu, else close.
void goBack(Player& player, json const& args) {
    auto from = strArg(args, "from");
    json back = json::object();
    if (args.contains("source")) back["source"] = args["source"];
    if (args.contains("uid")) back["uid"] = args["uid"];
    log().info("groups: {} left manage_groups (back to {})", playerName(player), from.empty() ? "selection" : from);
    if (!from.empty() && from != kManageScreen && from != kEditScreen && service::openScreen(player, from, back)) return;
    net::screens::close(player);
    if (auto it = args.find("source"); it != args.end() && it->is_object()) {
        service::get<service::ISelection>().open(player, core::WarpSource::fromJson(*it));
    }
}

std::string idArg(json const& args, char const* key = "id") { return strArg(args, key); }

void manageAction(Player& player, ScreenSession& session, std::string const& action, json const& args) {
    auto  ctxArgs = session.context.value("args", json::object());
    auto& data    = game::Waystones::get().playerData(player);
    auto  name    = playerName(player);
    if (action == "create") {
        openEdit(player, "", ctxArgs);
    } else if (action == "edit") {
        auto id = idArg(args);
        if (!cg::find(data.groupRegistry, id)) return log().warn("groups: {} tried to edit unknown group {}", name, id);
        openEdit(player, id, ctxArgs);
    } else if (action == "delete") {
        auto id = idArg(args);
        if (!cg::remove(data.groupRegistry, data.groups, id)) return log().warn("groups: {} cannot delete {}", name, id);
        saveData(player);
        log().info("groups: deleted group {} by {}", id, name);
        refreshManage(player);
    } else if (action == "hide") {
        auto id     = idArg(args);
        bool hidden = boolArg(args, "hidden", true);
        if (!cg::setHidden(data.groupRegistry, id, hidden)) return log().warn("groups: {} cannot hide {}", name, id);
        saveData(player);
        log().info("groups: {} inbuilt group {} by {}", hidden ? "hid" : "unhid", id, name);
        refreshManage(player);
    } else if (action == "swap") {
        auto a = idArg(args, "a"), b = idArg(args, "b");
        if (!cg::swap(data.groupRegistry, a, b)) return log().warn("groups: {} cannot swap {} and {}", name, a, b);
        saveData(player);
        log().info("groups: swapped {} with {} by {}", a, b, name);
        refreshManage(player);
    } else if (action == "move") {
        auto id    = idArg(args);
        auto index = intArg(args, "index", 0);
        if (!cg::moveTo(data.groupRegistry, id, index)) return log().warn("groups: {} cannot move {}", name, id);
        saveData(player);
        log().info("groups: moved {} to {} by {}", id, index, name);
        refreshManage(player);
    } else if (action == "back") {
        goBack(player, ctxArgs);
    }
}

void editAction(Player& player, ScreenSession& session, std::string const& action, json const& args) {
    auto ctxArgs = session.context.value("args", json::object());
    auto id      = session.context.value("group", std::string{});
    auto pname   = playerName(player);
    if (action == "back") {
        openManage(player, ctxArgs);
        return;
    }
    if (action != "save") return;
    auto& data  = game::Waystones::get().playerData(player);
    auto  name  = strArg(args, "name");
    auto  icon  = strArg(args, "icon");
    auto  color = strArg(args, "color");
    if (!cg::isIcon(icon) || !cg::isColor(color)) return log().warn("groups: {} sent an invalid icon/color ({}, {})", pname, icon, color);
    if (id.empty()) {
        id = cg::create(data.groupRegistry, name, icon, color, core::randomUuid());
        if (id.empty()) return log().warn("groups: {} has too many groups", pname);
        saveData(player);
        log().info("groups: created group {} '{}' icon={} color={} by {}", id, cg::sanitizeName(name), icon, color, pname);
    } else {
        // Saving an inbuilt group with its (translated) default name keeps it translatable.
        std::string defaultName;
        for (auto const& d : cg::inbuiltDefaults()) {
            if (d.id == id) defaultName = trFor(player, cg::nameKey(id), d.name);
        }
        if (!cg::update(data.groupRegistry, id, name, icon, color, defaultName)) {
            return log().warn("groups: {} cannot update {}", pname, id);
        }
        saveData(player);
        log().info("groups: updated group {} '{}' icon={} color={} by {}", id, cg::sanitizeName(name), icon, color, pname);
    }
    openManage(player, ctxArgs);
}

// ---- form fallbacks ---------------------------------------------------------------------------------

std::string formatCode(std::string_view color) {
    static constexpr std::array<std::pair<std::string_view, char>, 16> kCodes{{
        {"black", '0'},     {"dark_blue", '1'}, {"dark_green", '2'},   {"dark_aqua", '3'},
        {"dark_red", '4'},  {"dark_purple", '5'}, {"gold", '6'},       {"gray", '7'},
        {"dark_gray", '8'}, {"blue", '9'},      {"green", 'a'},        {"aqua", 'b'},
        {"red", 'c'},       {"light_purple", 'd'}, {"yellow", 'e'},    {"white", 'f'},
    }};
    for (auto const& [n, c] : kCodes) {
        if (n == color) return std::string("§") + c;
    }
    return "§f";
}

std::string iconName(Player const& p, std::string_view icon) { return trFor(p, cg::iconKey(icon), cg::displayName(icon)); }
std::string colorName(Player const& p, std::string_view color) { return trFor(p, cg::colorKey(color), cg::displayName(color)); }

void showGroupForm(Player& player, std::uint32_t session, json const& group, int index, int count);

void showManageForm(Player& player, ScreenSession& session) {
    auto const groups = session.model.value("groups", json::array());
    auto       sid    = session.session;
    ll::form::SimpleForm form(trFor(player, "gui.waystones.manage_groups.title", "Manage Groups"));
    if (groups.empty()) form.setContent("§c" + trFor(player, "gui.waystones.manage_groups.no_groups", "There are no groups available."));
    form.appendButton("+ " + trFor(player, "gui.waystones.manage_groups.create_group", "Create Group"));
    for (auto const& g : groups) {
        auto label = formatCode(g.value("color", "white")) + g.value("name", std::string{});
        if (g.value("hidden", false)) label += " §7(" + trFor(player, "bs.groups.hidden", "hidden") + ")";
        form.appendButton(label);
    }
    form.appendButton(trFor(player, "gui.waystones.manage_groups.back", "Back"));
    form.sendTo(player, [sid, groups](Player& p, int index, ll::form::FormCancelReason) {
        if (!net::screens::current(p, kManageScreen, sid)) return;
        auto n = static_cast<int>(groups.size());
        if (index < 0) return net::screens::simulateAction(p, "close");
        if (index == 0) return net::screens::simulateAction(p, "create");
        if (index == n + 1) return net::screens::simulateAction(p, "back");
        showGroupForm(p, sid, groups[static_cast<std::size_t>(index - 1)], index - 1, n);
    });
}

void showGroupForm(Player& player, std::uint32_t session, json const& group, int index, int count) {
    auto id      = group.value("id", std::string{});
    bool inbuilt = group.value("inbuilt", false);
    bool hidden  = group.value("hidden", false);
    ll::form::SimpleForm form(group.value("name", std::string{}));
    std::vector<std::pair<std::string, json>> actions;
    auto add = [&](std::string label, std::string action, json args) {
        form.appendButton(std::move(label));
        actions.emplace_back(std::move(action), std::move(args));
    };
    add(trFor(player, "gui.waystones.manage_groups.edit_group", "Edit Group"), "edit", {{"id", id}});
    if (index > 0) add(trFor(player, "bs.groups.move_up", "Move Up"), "move", {{"id", id}, {"index", index - 1}});
    if (index + 1 < count) add(trFor(player, "bs.groups.move_down", "Move Down"), "move", {{"id", id}, {"index", index + 1}});
    if (inbuilt) {
        add(hidden ? trFor(player, "gui.waystones.manage_groups.unhide_inbuilt_group", "Unhide Inbuilt Group")
                   : trFor(player, "gui.waystones.manage_groups.hide_inbuilt_group", "Hide Inbuilt Group"),
            "hide",
            {{"id", id}, {"hidden", !hidden}});
    } else {
        add("§c" + trFor(player, "gui.waystones.manage_groups.delete", "Delete"), "delete", {{"id", id}});
    }
    add(trFor(player, "gui.waystones.manage_groups.back", "Back"), "", json::object());
    form.sendTo(player, [session, actions, id](Player& p, int i, ll::form::FormCancelReason) {
        auto* s = net::screens::current(p, kManageScreen, session);
        if (!s) return;
        if (i < 0 || i >= static_cast<int>(actions.size()) || actions[static_cast<std::size_t>(i)].first.empty()) {
            showManageForm(p, *s); // back to the list
            return;
        }
        auto const& [action, args] = actions[static_cast<std::size_t>(i)];
        if (action != "delete") return net::screens::simulateAction(p, action, args);
        // The native screen needs Shift held to delete; the form asks for a confirmation instead.
        ll::form::ModalForm confirm(
            trFor(p, "gui.waystones.manage_groups.delete", "Delete"),
            trFor(p, "bs.groups.confirm_delete", "Delete this group? Waystones in it are not deleted."),
            trFor(p, "gui.waystones.manage_groups.delete", "Delete"),
            trFor(p, "gui.waystones.manage_groups.back", "Back")
        );
        confirm.sendTo(p, [session, args](Player& p2, ll::form::ModalFormResult r, ll::form::FormCancelReason) {
            auto* s2 = net::screens::current(p2, kManageScreen, session);
            if (!s2) return;
            if (r && *r == ll::form::ModalFormSelectedButton::Upper) net::screens::simulateAction(p2, "delete", args);
            else showManageForm(p2, *s2);
        });
    });
}

void showEditForm(Player& player, ScreenSession& session) {
    auto const& m   = session.model;
    auto        sid = session.session;
    std::vector<std::string> icons, colors;
    std::size_t              iconIdx = 0, colorIdx = 0;
    for (std::size_t i = 0; i < cg::kIcons.size(); ++i) {
        icons.push_back(iconName(player, cg::kIcons[i]));
        if (cg::kIcons[i] == m.value("icon", std::string{})) iconIdx = i;
    }
    for (std::size_t i = 0; i < cg::kColors.size(); ++i) {
        colors.push_back(formatCode(cg::kColors[i]) + colorName(player, cg::kColors[i]));
        if (cg::kColors[i] == m.value("color", std::string{})) colorIdx = i;
    }
    ll::form::CustomForm form(trFor(player, "gui.waystones.group_settings.title", "Edit Group"));
    form.appendInput(
        "name",
        trFor(player, "bs.groups.name", "Name"),
        trFor(player, "gui.waystones.group_settings.name_placeholder", "Unnamed Group"),
        m.value("name", std::string{})
    );
    form.appendDropdown("icon", trFor(player, "bs.groups.icon", "Icon"), icons, iconIdx);
    form.appendDropdown("color", trFor(player, "bs.groups.color", "Label Color"), colors, colorIdx);
    form.sendTo(player, [sid](Player& p, ll::form::CustomFormResult const& res, ll::form::FormCancelReason) {
        if (!net::screens::current(p, kEditScreen, sid)) return;
        if (!res) return net::screens::simulateAction(p, "back");
        json args = json::object();
        if (auto it = res->find("name"); it != res->end()) {
            if (auto const* s = std::get_if<std::string>(&it->second)) args["name"] = *s;
        }
        auto pick = [&](char const* key, auto const& list) {
            if (auto it = res->find(key); it != res->end()) {
                if (auto const* idx = std::get_if<uint64>(&it->second); idx && *idx < list.size()) args[key] = std::string(list[*idx]);
            }
        };
        pick("icon", cg::kIcons);
        pick("color", cg::kColors);
        net::screens::simulateAction(p, "save", args);
    });
}

} // namespace

void openManage(Player& player, json const& args) {
    auto ctxArgs = returnArgsOf(args);
    auto session = net::screens::open(player, kManageScreen, manageModel(player), {{"args", ctxArgs}});
    if (session) log().info("groups: manage_groups opened for {}", playerName(player));
}

void openEdit(Player& player, std::string const& groupId, json const& returnArgs) {
    auto& data = game::Waystones::get().playerData(player);
    json  model{{"id", groupId}, {"new", groupId.empty()}, {"name", ""}, {"icon", std::string(cg::kIcons.front())}, {"color", "white"}, {"inbuilt", false}};
    if (!groupId.empty()) {
        auto g = cg::find(data.groupRegistry, groupId, translatorFor(player));
        if (!g) {
            log().warn("groups: edit_group: unknown group {}", groupId);
            return;
        }
        model["name"]    = g->name;
        model["icon"]    = g->icon;
        model["color"]   = g->color;
        model["inbuilt"] = g->inbuilt;
    }
    auto session = net::screens::open(player, kEditScreen, model, {{"group", groupId}, {"args", returnArgsOf(returnArgs)}});
    if (session) log().info("groups: edit_group opened ({}) for {}", groupId.empty() ? "new" : groupId, playerName(player));
}

} // namespace bs::feature::groups

namespace {
namespace fg = bs::feature::groups;
}

BS_REGISTER_SCREEN_ACTIONS(fg::kManageScreen, fg::manageAction);
BS_REGISTER_SCREEN_ACTIONS(fg::kEditScreen, fg::editAction);
BS_REGISTER_SCREEN_FALLBACK(fg::kManageScreen, fg::showManageForm);
BS_REGISTER_SCREEN_FALLBACK(fg::kEditScreen, fg::showEditForm);

BS_REGISTER_SCREEN_OPENER("manage_groups", [](Player& p, nlohmann::json const& args) { fg::openManage(p, args); });
BS_REGISTER_SCREEN_OPENER("edit_group", [](Player& p, nlohmann::json const& args) {
    fg::openEdit(p, fg::strArg(args, "group"), args);
});

BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload().text("debug").text("groups").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = bs::commands::playerOf(origin);
        if (!player) return output.error("Only players can open screens.");
        if (!bs::commands::isAdmin(origin)) return output.error("You need operator permissions.");
        fg::openManage(*player, nlohmann::json::object());
        output.success("manage_groups opened");
    });
});
