// ISelection: the "selection" / "admin_selection" screens (docs/SPEC.md §4.1): model, actions,
// form fallback and the screen openers other units use.

#include <algorithm>
#include <climits>
#include <cmath>
#include <map>

#include "ll/api/form/SimpleForm.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/item/ItemStack.h"

#include "core/selection/Selection.h"
#include "feature/selection/Selection.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "net/Screens.h"
#include "service/ScreenOpeners.h"
#include "service/Services.h"

namespace bs::feature::selection {

// ---- player state --------------------------------------------------------------------------------

int playerLevel(Player& p) {
    auto ref = p.getAttribute(Player::LEVEL());
    return ref.mPtr ? static_cast<int>(ref.mPtr->mCurrentValue) : 0;
}

float playerProgress(Player& p) {
    auto ref = p.getAttribute(Player::EXPERIENCE());
    return ref.mPtr ? ref.mPtr->mCurrentValue : 0.f;
}

std::string fullItemId(std::string const& id) { return id.find(':') == std::string::npos ? "minecraft:" + id : id; }

int countItem(Player& p, std::string const& id) {
    auto want  = fullItemId(id);
    int  total = 0;
    int  size  = p.getInventory().getContainerSize();
    for (int i = 0; i < size; ++i) {
        auto const& s = game::itemAt(p, {game::ItemLocation::Kind::Inventory, i});
        if (!s.isNull() && game::idOf(s) == want) total += s.mCount;
    }
    auto const& off = game::itemAt(p, {game::ItemLocation::Kind::Offhand, 0});
    if (!off.isNull() && game::idOf(off) == want) total += off.mCount;
    return total;
}

namespace {

namespace sel = core::selection;
using core::WarpTarget;
using nlohmann::json;

/// "$key" refuse messages are translation keys.
std::string refuseText(Player& p, std::string const& refuse) {
    if (refuse.starts_with('$')) return trFor(p, refuse.substr(1), refuse.substr(1));
    return refuse;
}

sel::SortMode sortModeOf(Player& p) {
    return sel::sortModeFromString(game::Waystones::get().playerData(p).sortMode).value_or(sel::SortMode::Manual);
}

// ---- model -----------------------------------------------------------------------------------------

/// Player state read once per model.
struct Wallet {
    int                        level    = 0;
    float                      progress = 0;
    std::map<std::string, int> items; ///< item id -> count carried (filled lazily)
};

json requirementJson(Player& p, Wallet& wallet, WarpTarget const& t) {
    auto const& r = t.requirement;
    json        items = json::array();
    for (auto const& i : r.items) {
        auto id = fullItemId(i.id);
        auto it = wallet.items.find(id);
        if (it == wallet.items.end()) it = wallet.items.emplace(id, countItem(p, id)).first;
        items.push_back({{"id", id}, {"count", i.count}, {"missing", it->second < i.count}});
    }
    int levelDisplay = std::max(0, r.levels);
    if (r.xpPoints > 0) levelDisplay += core::levelsForPoints(wallet.level, wallet.progress, r.xpPoints);
    return {
        {"xp", r.xpPoints},
        {"levels", r.levels},
        {"levelDisplay", levelDisplay},
        {"canAfford", t.canAfford},
        {"cooldown", r.activeCooldown},
        {"items", items},
        {"refuse", r.refuse.empty() ? std::string{} : refuseText(p, r.refuse)},
    };
}

std::vector<sel::GroupInfo> groupInfos(Player& p) {
    std::vector<sel::GroupInfo> out;
    for (auto const& g : service::get<service::IGroups>().groups(p)) out.push_back({g.id, g.name, g.icon, g.color, g.hidden});
    return out;
}

} // namespace

json buildModel(Player& player, core::WarpSource const& source, bool admin, std::string const& ownerUuid) {
    auto& ws      = game::Waystones::get();
    auto  targets = admin ? adminTargets(player, ownerUuid) : service::get<service::ITargets>().list(player, source);
    auto  mode    = admin ? sel::SortMode::Manual : sortModeOf(player);
    auto  pos     = player.getPosition();
    sel::Viewpoint view{static_cast<int>(player.getDimensionId()), pos.x, pos.y, pos.z};
    std::vector<std::string> index = admin ? ws.playerData(ownerUuid).sortingIndex : ws.playerData(player).sortingIndex;
    sel::sortTargets(targets, mode, index, view);

    bool   creative = player.isCreative();
    auto   groups   = groupInfos(player);
    Wallet wallet{playerLevel(player), playerProgress(player), {}};

    json entries = json::array();
    bool anyOther = false;
    for (auto const& t : targets) {
        if (!t.current) anyOther = true;
        bool sameDim = t.pos.dim == view.dim && t.kind != "player";
        entries.push_back({
            {"id", t.id},
            {"kind", t.kind},
            {"name", t.name},
            {"dim", t.pos.dim},
            {"distance", sameDim ? sel::distanceTo(view, t.pos) : -1.0},
            {"current", t.current},
            {"selectable", sel::isSelectable(t, creative)},
            {"pinned", sel::isPinned(t)},
            {"global", t.global},
            {"color", sel::labelColor(t, groups)},
            {"groups", t.groups},
            {"req", requirementJson(player, wallet, t)},
        });
    }
    json groupsJson = json::array();
    for (auto const& g : groups) {
        groupsJson.push_back({{"id", g.id}, {"name", g.name}, {"icon", g.icon}, {"color", g.color}, {"hidden", g.hidden}});
    }

    json model;
    model["admin"] = admin;
    if (admin) {
        model["title"] = trFor(
            player,
            "gui.waystones.waystone_selection.admin_title",
            "Select which waystone of {} you would like to visit:",
            playerName(ownerUuid)
        );
    } else {
        model["title"] = trFor(player, "gui.waystones.waystone_selection.title", "Select your destination");
    }
    auto* from           = admin ? nullptr : ws.find(source.fromUid);
    model["hasSource"]   = from != nullptr;
    model["currentName"] = from ? ws.playerData(player).effectiveName(from->uid, from->name) : std::string{};
    model["sortMode"]    = std::string(sel::toString(mode));
    model["showSort"]    = !admin;
    model["showManage"]  = !admin;
    model["canManage"]   = service::hasScreen("manage");
    bool sharestones     = source.kind == core::WarpSourceKind::Sharestone
                     || (source.kind == core::WarpSourceKind::Portstone && !source.portstoneColor.empty() && source.portstoneColor != "white");
    model["emptyText"] = anyOther ? std::string{}
                       : sharestones
                           ? trFor(player, "gui.waystones.waystone_selection.no_sharestones", "There are no matching Sharestones to teleport to.")
                           : trFor(player, "gui.waystones.waystone_selection.no_waystones_activated", "You have not activated any waystones yet.");
    model["groups"]    = groupsJson;
    model["entries"]   = entries;
    model["filter"]    = std::string{}; // the active group filter is client state
    return model;
}

void openSelection(Player& player, core::WarpSource const& source) {
    auto model = buildModel(player, source, false, {});
    auto n     = model["entries"].size();
    auto sid   = net::screens::open(player, kScreen, std::move(model), {{"source", source.toJson()}, {"admin", false}});
    log().info("selection: opened for {} ({} entries, source {}, session {})", player.getRealName(), n, core::toString(source.kind), sid);
}

void openAdminSelection(Player& viewer, std::string const& ownerUuid) {
    core::WarpSource src;
    src.kind   = core::WarpSourceKind::Admin;
    auto model = buildModel(viewer, src, true, ownerUuid);
    auto n     = model["entries"].size();
    auto sid   = net::screens::open(viewer, kAdminScreen, std::move(model), {{"source", src.toJson()}, {"admin", true}, {"owner", ownerUuid}});
    log().info("selection: admin view of {} opened for {} ({} entries, session {})", playerName(ownerUuid), viewer.getRealName(), n, sid);
}

namespace {

// ---- actions ---------------------------------------------------------------------------------------

void onAction(Player& player, net::screens::ScreenSession& session, std::string const& action, json const& args) {
    auto source = core::WarpSource::fromJson(session.context.value("source", json::object()));
    bool admin  = session.context.value("admin", false);
    auto owner  = session.context.value("owner", std::string{});

    if (action == "select") {
        auto id = args.is_object() ? args.value("id", std::string{}) : std::string{};
        // Never trust the client: the id must be in the freshly computed list and selectable.
        auto list = admin ? adminTargets(player, owner) : service::get<service::ITargets>().list(player, source);
        auto it   = std::find_if(list.begin(), list.end(), [&](WarpTarget const& t) { return t.id == id; });
        if (id.empty() || it == list.end() || !sel::isSelectable(*it, player.isCreative())) {
            log().info("selection: rejected {} for {}", id.empty() ? "<none>" : id, player.getRealName());
            if (session.fallback) net::screens::close(player);
            return;
        }
        auto target = *it;
        net::screens::close(player);
        log().info("selection: {} selected {} ({})", player.getRealName(), target.name, target.id);
        service::WarpRequest req;
        req.source           = admin ? core::WarpSource{.kind = core::WarpSourceKind::Admin} : source;
        req.targetId         = target.id;
        req.skipRequirements = admin;
        service::get<service::ITeleport>().warp(player, req);
        return;
    }
    if (action == "sort") {
        if (admin) return;
        auto& ws   = game::Waystones::get();
        auto& data = ws.playerData(player);
        auto  cur  = sel::sortModeFromString(data.sortMode).value_or(sel::SortMode::Manual);
        auto  want = args.is_object() ? sel::sortModeFromString(args.value("mode", std::string{})) : std::nullopt;
        auto  next = want.value_or(sel::nextSortMode(cur));
        data.sortMode = std::string(sel::toString(next));
        ws.markPlayerDirty(game::Waystones::playerKey(player));
        log().info("selection: {} sort mode {}", player.getRealName(), data.sortMode);
        net::screens::update(player, buildModel(player, source, false, {}));
        return;
    }
    if (action == "manage") {
        if (admin) return;
        bool opened = service::openScreen(player, "manage", {{"source", source.toJson()}});
        log().info("selection: {} manage ({})", player.getRealName(), opened ? "opened" : "unavailable");
        return;
    }
    if (action == "edit_current") {
        if (admin || !game::Waystones::get().find(source.fromUid)) return;
        log().info("selection: {} edit_current {}", player.getRealName(), source.fromUid);
        service::get<service::IEditScreen>().open(player, source.fromUid);
        return;
    }
    if (action == "close") log().info("selection: closed by {}", player.getRealName());
}

BS_REGISTER_SCREEN_ACTIONS(kScreen, onAction);
BS_REGISTER_SCREEN_ACTIONS(kAdminScreen, onAction);

// ---- form fallback -----------------------------------------------------------------------------------

char const* formatCode(std::string const& color) {
    static constexpr std::pair<char const*, char const*> kCodes[]{
        {"black", "§0"},  {"dark_blue", "§1"}, {"dark_green", "§2"}, {"dark_aqua", "§3"},    {"dark_red", "§4"},
        {"dark_purple", "§5"}, {"gold", "§6"}, {"gray", "§7"},       {"dark_gray", "§8"},    {"blue", "§9"},
        {"green", "§a"},  {"aqua", "§b"},      {"red", "§c"},        {"light_purple", "§d"}, {"yellow", "§e"},
        {"white", "§f"},
    };
    for (auto const& [n, c] : kCodes) {
        if (color == n) return c;
    }
    return "";
}

std::string costLine(Player& p, json const& e) {
    auto const&              req = e["req"];
    std::vector<std::string> parts;
    if (auto r = req.value("refuse", std::string{}); !r.empty()) parts.push_back("§c" + r);
    if (int lv = req.value("levelDisplay", 0); lv > 0) {
        parts.push_back((req.value("canAfford", true) ? "§a" : "§8") + trFor(p, "bs.selection.form.levels", "{} Levels", lv));
    }
    if (double cd = req.value("cooldown", 0.0); cd > 0) parts.push_back("§c" + sel::formatCooldown(cd));
    for (auto const& i : req.value("items", json::array())) {
        parts.push_back((i.value("missing", false) ? "§8" : "§f") + std::to_string(i.value("count", 0)) + "x " + i.value("id", std::string{}));
    }
    if (double d = e.value("distance", -1.0); d >= 0) parts.push_back("§7" + sel::formatDistance(d));
    std::string out;
    for (auto const& s : parts) out += (out.empty() ? "" : " §8| ") + s;
    return out;
}

void showFallback(Player& player, net::screens::ScreenSession& session) {
    auto const& m = session.model;
    ll::form::SimpleForm form(m.value("title", std::string{}));
    std::string content;
    if (m.value("hasSource", false)) {
        auto name = m.value("currentName", std::string{});
        content += "§e" + trFor(player, "gui.waystones.waystone_selection.current_location", "Current Location: ") + "§f"
                 + (name.empty() ? trFor(player, "bs.selection.unnamed", "<unnamed>") : name) + "\n";
    }
    if (auto e = m.value("emptyText", std::string{}); !e.empty()) content += "§c" + e;
    form.setContent(content);

    std::vector<std::pair<std::string, json>> actions; // button -> action
    for (auto const& e : m.value("entries", json::array())) {
        auto name = e.value("name", std::string{});
        if (name.empty()) name = trFor(player, "bs.selection.unnamed", "<unnamed>");
        bool        ok    = e.value("selectable", false);
        std::string label = (ok ? std::string(formatCode(e.value("color", std::string{"white"}))) : std::string("§8")) + name;
        if (e.value("current", false)) label += " §8(" + trFor(player, "bs.selection.form.current", "current") + ")";
        if (auto line = costLine(player, e); !line.empty()) label += "\n" + line;
        form.appendButton(label);
        actions.emplace_back("select", json{{"id", e.value("id", std::string{})}});
    }
    if (m.value("showSort", false)) {
        auto mode = m.value("sortMode", std::string{"manual"});
        form.appendButton(trFor(
            player,
            "gui.waystones.waystone_selection.sort",
            "Sort: {}",
            trFor(player, "gui.waystones.waystone_selection.sort." + mode, mode)
        ));
        actions.emplace_back("sort", json::object());
    }
    if (m.value("showManage", false) && m.value("canManage", false)) {
        form.appendButton(trFor(player, "gui.waystones.waystone_selection.manage_waystones", "Manage Waystones"));
        actions.emplace_back("manage", json::object());
    }
    if (m.value("hasSource", false)) {
        form.appendButton(trFor(player, "bs.selection.form.edit_current", "Edit current waystone"));
        actions.emplace_back("edit_current", json::object());
    }
    auto session_ = session.session;
    form.sendTo(player, [actions = std::move(actions), session_](Player& p, int index, ll::form::FormCancelReason) {
        if (!net::screens::current(p, kScreen, session_) && !net::screens::current(p, kAdminScreen, session_)) return;
        if (index < 0 || index >= static_cast<int>(actions.size())) {
            net::screens::simulateAction(p, "close");
            return;
        }
        auto const& [action, args] = actions[static_cast<std::size_t>(index)];
        net::screens::simulateAction(p, action, args);
        // Actions that leave this screen in place (manage unavailable, edit form) end the form session.
        if (action != "sort" && net::screens::current(p, kScreen, session_)) net::screens::close(p);
        if (action != "sort" && net::screens::current(p, kAdminScreen, session_)) net::screens::close(p);
    });
}

BS_REGISTER_SCREEN_FALLBACK(kScreen, showFallback);
BS_REGISTER_SCREEN_FALLBACK(kAdminScreen, showFallback);

// ---- seams -----------------------------------------------------------------------------------------

class Targets final : public service::ITargets {
public:
    std::vector<WarpTarget> list(Player& player, core::WarpSource const& source) override { return computeTargets(player, source); }
};

class Selection final : public service::ISelection {
public:
    void open(Player& player, core::WarpSource const& source) override { openSelection(player, source); }
    void openAdmin(Player& viewer, std::string const& ownerUuid) override { openAdminSelection(viewer, ownerUuid); }
};

BS_REGISTER_SCREEN_OPENER("selection", [](Player& player, json const& args) {
    auto src = core::WarpSource::fromJson(args.is_object() ? args.value("source", json::object()) : json::object());
    service::get<service::ISelection>().open(player, src);
});

BS_REGISTER_SCREEN_OPENER("admin_selection", [](Player& player, json const& args) {
    auto owner = args.is_object() ? args.value("owner", std::string{}) : std::string{};
    if (owner.empty()) owner = game::Waystones::playerKey(player);
    service::get<service::ISelection>().openAdmin(player, owner);
});

class SelectionModule {
public:
    void install() {
        service::set<service::ITargets>(std::make_unique<Targets>());
        service::set<service::ISelection>(std::make_unique<Selection>());
    }
    void uninstall() {
        setRequirementPreview(false);
        service::reset<service::ISelection>();
        service::reset<service::ITargets>();
    }
};

} // namespace

} // namespace bs::feature::selection

BS_REGISTER_MODULE(bs::feature::selection::SelectionModule);
