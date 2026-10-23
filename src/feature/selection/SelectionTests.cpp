// Selftests of U10 (selection screen): target sets per source, sorting, select validation,
// the admin variant and the screen model. Test positions use dx 1000..1099.

#include <algorithm>
#include <cmath>

#include "mc/world/level/dimension/DimensionType.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/server/SimulatedPlayer.h"
#include "mc/world/actor/player/Player.h"

#include "core/selection/Selection.h"
#include "feature/selection/Selection.h"
#include "game/BlockContainer.h"
#include "game/Ids.h"
#include "game/Waystones.h"
#include "net/Companion.h"
#include "net/Screens.h"
#include "selftest/SelfTest.h"
#include "service/ScreenOpeners.h"
#include "service/Services.h"
#include "service/TargetProviders.h"

namespace bs::feature::selection {

namespace {

using core::WarpSource;
using core::WarpSourceKind;
using core::WarpTarget;
using nlohmann::json;
using selftest::Context;

// A provider that only lists entries while a test enables it.
bool gTestProvider = false;

BS_REGISTER_TARGET_PROVIDER(service::TargetProvider{
    .name = "selection_test",
    .list =
        [](Player&, WarpSource const&, std::vector<WarpTarget>& out) {
            if (!gTestProvider) return;
            WarpTarget m;
            m.id   = "seltest:memorial";
            m.kind = "fleeting_memorial";
            m.name = "aaa memorial";
            out.push_back(m);
            WarpTarget p;
            p.id   = "seltest:player";
            p.kind = "player";
            p.name = "Partner";
            out.push_back(p);
        },
    .resolve = [](Player&, std::string const&) -> std::optional<service::Destination> { return std::nullopt; },
    .owns    = [](std::string const& id) { return id.starts_with("seltest:"); },
});

core::WaystoneRecord& make(Context& ctx, int dx, std::string name, std::string type = "waystone", core::Visibility vis = core::Visibility::Activation) {
    core::WaystoneRecord r;
    r.name       = std::move(name);
    r.type       = std::move(type);
    r.pos        = ctx.testPos(dx);
    r.style      = r.type;
    r.origin     = core::Origin::Player;
    r.visibility = core::types::isSharestone(r.type) ? core::Visibility::Sharestone : vis;
    return ctx.createWaystone(std::move(r), true);
}

WarpSource sourceAt(WarpSourceKind kind, core::WaystoneRecord const* from = nullptr) {
    WarpSource s;
    s.kind = kind;
    if (from) {
        s.fromUid = from->uid;
        s.fromPos = from->pos;
    }
    return s;
}

WarpTarget const* findIn(std::vector<WarpTarget> const& v, std::string const& id) {
    auto it = std::find_if(v.begin(), v.end(), [&](auto const& t) { return t.id == id; });
    return it == v.end() ? nullptr : &*it;
}

std::vector<WarpTarget> listFor(Player& p, WarpSource const& s) { return service::get<service::ITargets>().list(p, s); }

/// Captures screen packets of the test player.
struct Sink {
    std::vector<json> opens, updates;
    int               closes = 0;
    Player&           player;
    explicit Sink(Player& p) : player(p) {
        net::setCompanion(p, net::CompanionInfo{"test", {"native_screen"}});
        net::setTestSink([this](Player&, std::string_view, json const& body) {
            if (body.contains("id") && body.contains("model")) opens.push_back(body);
            else if (body.contains("model")) updates.push_back(body);
            else if (body.contains("session")) ++closes;
        });
    }
    ~Sink() {
        net::screens::close(player);
        net::setTestSink(nullptr);
        net::setCompanion(player, std::nullopt);
    }
    [[nodiscard]] json const& model() const { return updates.empty() ? opens.back()["model"] : updates.back()["model"]; }
};

/// Entry names of the model restricted to `ids` (in model order).
std::vector<std::string> namesOf(json const& model, std::vector<std::string> const& ids) {
    std::vector<std::string> out;
    for (auto const& e : model["entries"]) {
        auto id = e.value("id", std::string{});
        if (std::find(ids.begin(), ids.end(), id) != ids.end()) out.push_back(e.value("name", std::string{}));
    }
    return out;
}

void setSortMode(Player& p, std::string mode) {
    game::Waystones::get().playerData(p).sortMode = std::move(mode);
}

} // namespace

BS_REGISTER_SELFTEST("selection.targets_waystone", [](Context& ctx) {
    auto& ws  = game::Waystones::get();
    auto& p   = ctx.player();
    auto& a   = make(ctx, 1000, "Source");
    auto& b   = make(ctx, 1002, "Activated");
    auto& c   = make(ctx, 1004, "Unknown");
    auto& g   = make(ctx, 1006, "Global", "waystone", core::Visibility::Global);
    auto& h   = make(ctx, 1008, "Hidden");
    auto& s   = make(ctx, 1009, "Red", core::types::sharestone("red"));
    ws.activate(p, a.uid);
    ws.activate(p, b.uid);
    ws.activate(p, h.uid);
    ws.playerData(p).hidden.insert(h.uid);
    auto list = listFor(p, sourceAt(WarpSourceKind::Waystone, &a));
    ws.playerData(p).hidden.erase(h.uid);
    ctx.check(findIn(list, a.uid) && findIn(list, a.uid)->current, "the source waystone is listed and flagged current");
    ctx.check(findIn(list, b.uid) && !findIn(list, b.uid)->current, "activated waystones are listed");
    ctx.check(findIn(list, g.uid) != nullptr, "global waystones are listed without activation");
    ctx.check(!findIn(list, c.uid), "waystones the player did not activate are not listed");
    ctx.check(!findIn(list, h.uid), "hidden waystones are not listed");
    ctx.check(!findIn(list, s.uid), "sharestones are not listed from a waystone");
    if (auto* t = findIn(list, g.uid)) {
        ctx.check(std::find(t->groups.begin(), t->groups.end(), std::string(service::groups::kGlobal)) != t->groups.end(), "groups are resolved");
    }
});

BS_REGISTER_SELFTEST("selection.targets_sharestone", [](Context& ctx) {
    auto& ws  = game::Waystones::get();
    auto& p   = ctx.player();
    auto& r1  = make(ctx, 1010, "Red One", core::types::sharestone("red"));
    auto& r2  = make(ctx, 1012, "Red Two", core::types::sharestone("red"));
    auto& bl  = make(ctx, 1014, "Blue", core::types::sharestone("blue"));
    auto& w   = make(ctx, 1016, "Waystone");
    ws.activate(p, w.uid);
    auto list = listFor(p, sourceAt(WarpSourceKind::Sharestone, &r1));
    ctx.check(findIn(list, r1.uid) && findIn(list, r1.uid)->current, "the source sharestone is current");
    ctx.check(findIn(list, r2.uid) != nullptr, "same-colour sharestones are listed without activation");
    ctx.check(!findIn(list, bl.uid), "other colours are not listed");
    ctx.check(!findIn(list, w.uid), "waystones are not listed from a sharestone");
    auto lone = listFor(p, sourceAt(WarpSourceKind::Sharestone, &bl));
    auto model = buildModel(p, sourceAt(WarpSourceKind::Sharestone, &bl), false, {});
    ctx.check(lone.size() >= 1 && model.value("emptyText", std::string{}).find("Sharestones") != std::string::npos,
              "a lone sharestone shows the 'no matching Sharestones' message");
});

BS_REGISTER_SELFTEST("selection.targets_portstone", [](Context& ctx) {
    auto& ws = game::Waystones::get();
    auto& p  = ctx.player();
    auto& w  = make(ctx, 1018, "Waystone");
    auto& g  = make(ctx, 1020, "Global", "waystone", core::Visibility::Global);
    auto& r  = make(ctx, 1022, "Red", core::types::sharestone("red"));
    ws.activate(p, w.uid);
    auto white           = sourceAt(WarpSourceKind::Portstone);
    white.portstoneColor = "white";
    auto list            = listFor(p, white);
    ctx.check(findIn(list, w.uid) && findIn(list, g.uid), "the white portstone lists activated + global waystones");
    ctx.check(!findIn(list, r.uid), "the white portstone lists no sharestones");
    auto red           = sourceAt(WarpSourceKind::Portstone);
    red.portstoneColor = "red";
    auto list2         = listFor(p, red);
    ctx.check(findIn(list2, r.uid) && !findIn(list2, w.uid), "a coloured portstone lists that colour's sharestones");
});

BS_REGISTER_SELFTEST("selection.targets_items", [](Context& ctx) {
    auto& ws = game::Waystones::get();
    auto& p  = ctx.player();
    auto& w  = make(ctx, 1026, "Activated");
    auto& g  = make(ctx, 1028, "Global", "waystone", core::Visibility::Global);
    auto& u  = make(ctx, 1030, "Unknown");
    ws.activate(p, w.uid);
    for (auto kind : {WarpSourceKind::WarpScroll, WarpSourceKind::WarpStone, WarpSourceKind::PortalScroll, WarpSourceKind::InventoryButton}) {
        auto list = listFor(p, sourceAt(kind));
        auto name = std::string(core::toString(kind));
        ctx.check(findIn(list, w.uid) && findIn(list, g.uid), name + ": activated + global listed");
        ctx.check(!findIn(list, u.uid), name + ": unknown waystones not listed");
        ctx.check(std::none_of(list.begin(), list.end(), [](auto const& t) { return t.current; }), name + ": nothing is current");
    }
});

BS_REGISTER_SELFTEST("selection.targets_container", [](Context& ctx) {
    auto& ws    = game::Waystones::get();
    auto& p     = ctx.player();
    auto& src   = make(ctx, 1032, "Source");
    auto& plate = make(ctx, 1034, "Plate", "warp_plate", core::Visibility::ShardOnly);
    ws.activate(p, src.uid);
    auto c = game::containerOf(src);
    c[0]   = game::SlotItem{ids::id(ids::kAttunedShard), 1, 0, {{std::string(ids::kNbtAttunement), plate.uid}}};
    game::setContainer(src, c);
    ws.markDirty(src.uid);
    auto list = listFor(p, sourceAt(WarpSourceKind::Waystone, &src));
    ctx.check(findIn(list, plate.uid) != nullptr, "an attuned shard in the source's container adds its target");
    auto scroll = listFor(p, sourceAt(WarpSourceKind::WarpScroll));
    ctx.check(!findIn(scroll, plate.uid), "item sources do not list container targets");
});

BS_REGISTER_SELFTEST("selection.providers_pinned", [](Context& ctx) {
    auto& ws = game::Waystones::get();
    auto& p  = ctx.player();
    auto& a  = make(ctx, 1038, "Aardvark");
    ws.activate(p, a.uid);
    gTestProvider = true;
    auto list     = listFor(p, sourceAt(WarpSourceKind::WarpScroll));
    setSortMode(p, "name");
    auto model    = buildModel(p, sourceAt(WarpSourceKind::WarpScroll), false, {});
    gTestProvider = false;
    auto* mem     = findIn(list, "seltest:memorial");
    ctx.check(mem && mem->pinned, "provider entries are listed; memorials are pinned");
    ctx.check(findIn(list, "seltest:player") != nullptr, "player targets are listed");
    auto names = namesOf(model, {a.uid, "seltest:memorial", "seltest:player"});
    ctx.check(!names.empty() && names.front() == "aaa memorial", "pinned entries come first in every sort mode");
    for (auto const& e : model["entries"]) {
        if (e.value("id", std::string{}) == "seltest:player") ctx.check(e.value("distance", 0.0) < 0, "players show no distance");
    }
});

BS_REGISTER_SELFTEST("selection.sort", [](Context& ctx) {
    auto& ws = game::Waystones::get();
    auto& p  = ctx.player();
    // Distances from the player near the spawn: c < b < a.
    auto& a = make(ctx, 1048, "alpha");
    auto& b = make(ctx, 1046, "Bravo");
    auto& c = make(ctx, 1044, "charlie");
    std::vector<std::string> ids{a.uid, b.uid, c.uid};
    for (auto const& id : ids) ws.activate(p, id);
    auto& data        = ws.playerData(p);
    data.sortingIndex = {b.uid, c.uid, a.uid};
    setSortMode(p, "manual");
    Sink sink(p);
    openSelection(p, sourceAt(WarpSourceKind::WarpScroll));
    ctx.require(!sink.opens.empty(), "selection opened");
    ctx.check(namesOf(sink.model(), ids) == std::vector<std::string>{"Bravo", "charlie", "alpha"}, "manual order follows the sorting index");
    ctx.check(sink.model().value("sortMode", std::string{}) == "manual", "model has the sort mode");
    net::screens::simulateAction(p, "sort");
    ctx.check(ws.playerData(p).sortMode == "name", "sort cycles manual -> name and persists");
    ctx.require(!sink.updates.empty(), "the model is updated after sorting");
    ctx.check(namesOf(sink.model(), ids) == std::vector<std::string>{"alpha", "Bravo", "charlie"}, "name order is case-insensitive");
    net::screens::simulateAction(p, "sort");
    ctx.check(ws.playerData(p).sortMode == "distance", "sort cycles name -> distance");
    ctx.check(namesOf(sink.model(), ids) == std::vector<std::string>{"charlie", "Bravo", "alpha"}, "distance order is nearest first");
    net::screens::simulateAction(p, "sort");
    ctx.check(ws.playerData(p).sortMode == "manual", "sort cycles distance -> manual");
    net::screens::simulateAction(p, "sort", {{"mode", "distance"}});
    ctx.check(ws.playerData(p).sortMode == "distance", "an explicit mode is accepted");
    setSortMode(p, "manual");
});

BS_REGISTER_SELFTEST("selection.select_validation", [](Context& ctx) -> ll::coro::CoroTask<> {
    ctx.freeWarps();
    auto& ws  = game::Waystones::get();
    auto& p   = ctx.player();
    auto& src = make(ctx, 1050, "Here");
    auto& dst = make(ctx, 1054, "There");
    auto& no  = make(ctx, 1056, "Not activated");
    dst.facing = 2;
    ws.activate(p, src.uid);
    ws.activate(p, dst.uid);
    auto dstUid = dst.uid, srcUid = src.uid, noUid = no.uid;
    auto dstPos = dst.pos;
    // Stand next to the source waystone (the teleport checks the source is in reach).
    p.teleport(Vec3{static_cast<float>(src.pos.x) + 1.5f, static_cast<float>(src.pos.y), static_cast<float>(src.pos.z) + 0.5f}, DimensionType(0));
    co_await ctx.ticks(2);
    Sink sink(p);
    openSelection(p, sourceAt(WarpSourceKind::Waystone, &src));
    ctx.require(net::screens::current(p, kScreen) != nullptr, "selection open");
    auto* s = net::screens::current(p, kScreen);
    ctx.check(s->context.contains("source") && core::WarpSource::fromJson(s->context["source"]).fromUid == srcUid,
              "context carries the WarpSource");
    net::screens::simulateAction(p, "select", {{"id", noUid}});
    ctx.check(net::screens::current(p, kScreen) != nullptr, "an id that is not in the list is rejected");
    net::screens::simulateAction(p, "select", {{"id", "bogus"}});
    ctx.check(net::screens::current(p, kScreen) != nullptr, "an unknown id is rejected");
    net::screens::simulateAction(p, "select", {{"id", srcUid}});
    ctx.check(net::screens::current(p, kScreen) != nullptr, "the current waystone cannot be selected");
    net::screens::simulateAction(p, "select", {{"id", dstUid}});
    ctx.check(net::screens::current(p) == nullptr, "a valid selection closes the screen");
    co_await ctx.ticks(2);
    auto pos = p.getPosition();
    ctx.check(std::abs(pos.x - (static_cast<float>(dstPos.x) + 0.5f)) < 1.5f && std::abs(pos.z - (static_cast<float>(dstPos.z - 1) + 0.5f)) < 1.5f,
              "the player was warped to the selected waystone");
});

BS_REGISTER_SELFTEST("selection.unaffordable", [](Context& ctx) {
    auto& ws = game::Waystones::get();
    auto& p  = ctx.player();
    auto& a  = make(ctx, 1070, "Sealed");
    auto& b  = make(ctx, 1072, "Cooling");
    a.extra["selectionPreview"] = {{"refuse", "$chat.waystones.cannot_dimension_warp"}};
    b.extra["selectionPreview"] = {{"activeCooldown", 90}, {"xpPoints", 5}};
    ws.activate(p, a.uid);
    ws.activate(p, b.uid);
    struct PreviewGuard {
        bool was = requirementPreview();
        PreviewGuard() { setRequirementPreview(true); }
        ~PreviewGuard() { setRequirementPreview(was); }
    } guard;
    ctx.check(requirementPreview(), "preview on");
    Sink sink(p);
    openSelection(p, sourceAt(WarpSourceKind::WarpScroll));
    ctx.require(!sink.opens.empty(), "selection opened");
    bool sawA = false, sawB = false;
    for (auto const& e : sink.model()["entries"]) {
        auto const& req = e["req"];
        if (e.value("id", std::string{}) == a.uid) {
            sawA = true;
            ctx.check(!e.value("selectable", true), "refused entries are disabled");
            ctx.check(!req.value("refuse", std::string{}).empty() && req.value("refuse", std::string{})[0] != '$', "refuse key is translated");
        }
        if (e.value("id", std::string{}) == b.uid) {
            sawB = true;
            ctx.check(!e.value("selectable", true) && !req.value("canAfford", true), "cooling down entries are disabled");
            ctx.check(req.value("cooldown", 0.0) == 90.0 && req.value("xp", 0) == 5 && req.value("levelDisplay", 0) >= 1, "requirement widgets data");
        }
    }
    ctx.check(sawA && sawB, "entries present");
    net::screens::simulateAction(p, "select", {{"id", a.uid}});
    ctx.check(net::screens::current(p, kScreen) != nullptr, "selecting an unaffordable entry is rejected");
});

BS_REGISTER_SELFTEST("selection.admin", [](Context& ctx) {
    auto& ws    = game::Waystones::get();
    auto& p     = ctx.player();
    auto& owner = ctx.spawnPlayer("bs_sel_owner");
    auto& w     = make(ctx, 1058, "Owner's Home");
    auto& x     = make(ctx, 1060, "Viewer's Own");
    ws.activate(owner, w.uid);
    ws.activate(p, x.uid);
    Sink sink(p);
    openAdminSelection(p, game::Waystones::playerKey(owner));
    ctx.require(net::screens::current(p, kAdminScreen) != nullptr, "admin selection open");
    auto const& m = sink.model();
    ctx.check(m.value("title", std::string{}).find(owner.getRealName()) != std::string::npos, "title names the owner");
    ctx.check(m.value("admin", false) && !m.value("showSort", true) && !m.value("showManage", true), "no sort / manage in the admin view");
    auto names = namesOf(m, {w.uid, x.uid});
    ctx.check(names == std::vector<std::string>{"Owner's Home"}, "lists the owner's waystones, not the viewer's");
    net::screens::simulateAction(p, "sort");
    ctx.check(sink.updates.empty(), "sort is ignored in the admin view");
    ctx.check(service::openScreen(p, "admin_selection", {{"owner", game::Waystones::playerKey(owner)}}), "admin_selection opener");
    net::screens::simulateAction(p, "select", {{"id", w.uid}});
    ctx.check(net::screens::current(p) == nullptr, "admin select warps and closes");
});

BS_REGISTER_SELFTEST("selection.model", [](Context& ctx) {
    auto& ws  = game::Waystones::get();
    auto& p   = ctx.player();
    auto& src = make(ctx, 1064, "Home Base");
    auto& fav = make(ctx, 1066, "Favourite");
    ws.activate(p, src.uid);
    ws.activate(p, fav.uid);
    ws.playerData(p).groups[fav.uid] = {std::string(service::groups::kFavorites)};
    Sink sink(p);
    ctx.check(service::hasScreen("selection") && service::hasScreen("admin_selection"), "screen openers registered");
    ctx.check(service::openScreen(p, "selection", {{"source", sourceAt(WarpSourceKind::Waystone, &src).toJson()}}), "selection opener");
    ws.playerData(p).groups.erase(fav.uid);
    ctx.require(!sink.opens.empty() && sink.opens.back().value("id", std::string{}) == kScreen, "ScreenOpen 'selection' sent");
    auto const& m = sink.model();
    ctx.check(m.value("title", std::string{}) == "Select your destination", "title");
    ctx.check(m.value("hasSource", false) && m.value("currentName", std::string{}) == "Home Base", "current location");
    ctx.check(m.value("showSort", false) && m.value("showManage", false), "sort + manage shown");
    ctx.check(m.value("canManage", true) == service::hasScreen("manage"), "manage enabled only when the manage screen exists");
    ctx.check(m["groups"].is_array() && !m["groups"].empty(), "group list");
    bool sawFav = false, sawCur = false;
    for (auto const& e : m["entries"]) {
        for (auto key : {"id", "kind", "name", "dim", "distance", "current", "selectable", "pinned", "global", "color", "groups", "req"}) {
            if (!e.contains(key)) ctx.check(false, std::string("entry field ") + key);
        }
        if (e.value("id", std::string{}) == fav.uid) {
            sawFav = true;
            ctx.check(e.value("color", std::string{}) == "gold", "favourites are labelled gold");
            ctx.check(e.value("distance", -1.0) >= 0, "same-dimension distance");
        }
        if (e.value("id", std::string{}) == src.uid) sawCur = e.value("current", false) && !e.value("selectable", true);
    }
    ctx.check(sawFav, "favourite entry");
    ctx.check(sawCur, "the source is current and disabled");
    auto* s = net::screens::current(p, kScreen);
    ctx.require(s != nullptr, "session open");
    ctx.check(core::WarpSource::fromJson(s->context["source"]).kind == WarpSourceKind::Waystone, "context source kind");
    net::screens::simulateAction(p, "manage");
    if (!service::hasScreen("manage")) ctx.check(net::screens::current(p, kScreen) != nullptr, "manage without the unit keeps the screen");
});

BS_REGISTER_SELFTEST("selection.fallback", [](Context& ctx) {
    auto& ws = game::Waystones::get();
    auto& p  = ctx.player();
    auto& a  = make(ctx, 1080, "Form Entry");
    ws.activate(p, a.uid);
    net::setCompanion(p, std::nullopt);
    service::get<service::ISelection>().open(p, sourceAt(WarpSourceKind::WarpScroll));
    auto* s = net::screens::current(p, kScreen);
    ctx.check(s && s->fallback, "players without the client mod get the form fallback");
    net::screens::simulateAction(p, "close");
    ctx.check(net::screens::current(p) == nullptr, "close ends the session");
});

} // namespace bs::feature::selection
