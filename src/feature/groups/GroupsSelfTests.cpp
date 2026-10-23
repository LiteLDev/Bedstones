// U12 Groups selftests (`/waystones selftest "groups.*"`). Test positions use dx 1200..1299.

#include <algorithm>

#include "mc/world/actor/player/Player.h"

#include "core/groups/GroupRegistry.h"
#include "feature/groups/Groups.h"
#include "game/Waystones.h"
#include "net/Companion.h"
#include "net/Screens.h"
#include "selftest/SelfTest.h"
#include "service/ScreenOpeners.h"
#include "service/Services.h"

namespace {

using bs::selftest::Context;
using nlohmann::json;
namespace cg = bs::core::groups;
namespace fg = bs::feature::groups;

/// Gives the test player empty group data and a companion (native screens to the test sink),
/// and restores both afterwards.
class GroupsFixture {
public:
    explicit GroupsFixture(Context& ctx) : mPlayer(ctx.player()) {
        auto& d    = data();
        mRegistry  = d.groupRegistry;
        mAssigned  = d.groups;
        d.groupRegistry.clear();
        d.groups.clear();
        bs::net::setTestSink([this](Player&, std::string_view packet, json const& body) {
            if (packet.find("Open") != std::string_view::npos) opened.push_back(body.value("id", std::string{}));
        });
        bs::net::setCompanion(mPlayer, bs::net::CompanionInfo{"test", {"native_screen"}});
    }
    ~GroupsFixture() {
        bs::net::screens::close(mPlayer);
        bs::net::setTestSink(nullptr);
        bs::net::setCompanion(mPlayer, std::nullopt);
        auto& d         = data();
        d.groupRegistry = mRegistry;
        d.groups        = mAssigned;
        bs::game::Waystones::get().markPlayerDirty(bs::game::Waystones::playerKey(mPlayer));
    }
    GroupsFixture(GroupsFixture const&)            = delete;
    GroupsFixture& operator=(GroupsFixture const&) = delete;

    bs::core::PlayerWaystoneData& data() { return bs::game::Waystones::get().playerData(mPlayer); }
    [[nodiscard]] std::string screen() {
        auto* s = bs::net::screens::current(mPlayer);
        return s ? s->id : std::string{};
    }
    json model() {
        auto* s = bs::net::screens::current(mPlayer);
        return s ? s->model : json::object();
    }
    void act(std::string const& action, json const& args = json::object()) { bs::net::screens::simulateAction(mPlayer, action, args); }
    std::vector<std::string> order() {
        std::vector<std::string> ids;
        for (auto const& g : bs::service::get<bs::service::IGroups>().groups(mPlayer)) ids.push_back(g.id);
        return ids;
    }
    std::optional<bs::core::WaystoneGroup> group(std::string const& id) {
        for (auto const& g : bs::service::get<bs::service::IGroups>().groups(mPlayer)) {
            if (g.id == id) return g;
        }
        return std::nullopt;
    }
    /// Creates a user group through the screens; returns its id.
    std::string createViaScreens(std::string const& name, std::string const& icon, std::string const& color) {
        auto before = order();
        bs::service::openScreen(mPlayer, "manage_groups", json::object());
        act("create");
        act("save", {{"name", name}, {"icon", icon}, {"color", color}});
        for (auto const& id : order()) {
            if (std::find(before.begin(), before.end(), id) == before.end()) return id;
        }
        return {};
    }

    std::vector<std::string> opened;

private:
    Player&                                         mPlayer;
    std::vector<bs::core::WaystoneGroup>            mRegistry;
    std::map<std::string, std::vector<std::string>> mAssigned;
};

} // namespace

BS_REGISTER_SELFTEST("groups.inbuilt", [](Context& ctx) {
    GroupsFixture f(ctx);
    auto          list = bs::service::get<bs::service::IGroups>().groups(ctx.player());
    std::vector<std::string> ids;
    for (auto const& g : list) ids.push_back(g.id);
    std::vector<std::string> expected{
        "waystones:favorites", "waystones:players", "waystones:global", "waystones:teams",
        "waystones:dimension/0", "waystones:dimension/2", "waystones:dimension/1",
    };
    ctx.check(ids == expected, "inbuilt groups in Waystones' order");
    ctx.require(list.size() == 7, "seven inbuilt groups");
    ctx.check(list[0].color == "gold" && list[0].sortIndex == -30, "favorites: gold, -30");
    ctx.check(list[1].color == "green" && list[1].sortIndex == -20, "players: green, -20");
    ctx.check(list[2].color == "yellow" && list[2].sortIndex == -10, "global: yellow, -10");
    ctx.check(list[3].color == "aqua" && list[3].sortIndex == -5, "teams: aqua, -5");
    ctx.check(list[4].color == "white" && list[5].color == "white" && list[6].color == "white", "dimension groups are white");
    ctx.check(!list[0].name.empty(), "inbuilt names are translated");
    ctx.check(std::all_of(list.begin(), list.end(), [](auto const& g) { return g.inbuilt && !g.hidden; }), "inbuilt, visible");
});

BS_REGISTER_SELFTEST("groups.create_edit_delete", [](Context& ctx) {
    GroupsFixture f(ctx);
    ctx.require(bs::service::openScreen(ctx.player(), "manage_groups", {{"from", "manage"}}), "manage_groups opener registered");
    ctx.check(f.screen() == "manage_groups", "manage_groups is open");
    ctx.check(f.model()["groups"].size() == 7, "model lists the inbuilt groups");
    f.act("create");
    ctx.require(f.screen() == "edit_group", "+ opens Edit Group");
    ctx.check(f.model().value("new", false), "edit model is a new group");
    f.act("save", {{"name", "Farms"}, {"icon", "villages"}, {"color", "green"}});
    ctx.check(f.screen() == "manage_groups", "save returns to Manage Groups");
    std::string id;
    auto        created = f.model();
    for (auto const& g : created["groups"]) {
        if (g.value("name", std::string{}) == "Farms") id = g.value("id", std::string{});
    }
    ctx.require(id.starts_with("waystones:user/") && id.size() > 20, "new group id is waystones:user/<uuid>");
    ctx.check(f.order().back() == id, "new group sorts last");
    auto g = f.group(id);
    ctx.check(g && g->icon == "villages" && g->color == "green" && !g->inbuilt, "icon / colour saved");
    ctx.check(bs::net::screens::current(ctx.player())->context["args"].value("from", "") == "manage", "return args kept");

    f.act("edit", {{"id", id}});
    ctx.require(f.screen() == "edit_group", "edit opens Edit Group");
    ctx.check(f.model().value("name", "") == "Farms" && f.model().value("icon", "") == "villages", "edit model has the group");
    f.act("save", {{"name", "Mines"}, {"icon", "resource_sites"}, {"color", "dark_gray"}});
    g = f.group(id);
    ctx.check(g && g->name == "Mines" && g->icon == "resource_sites" && g->color == "dark_gray", "edit saved");

    // Edit an inbuilt group: colour override, name stays translatable.
    f.act("edit", {{"id", "waystones:favorites"}});
    auto favName = f.model().value("name", std::string{});
    f.act("save", {{"name", favName}, {"icon", "favorites"}, {"color", "red"}});
    auto fav = f.group("waystones:favorites");
    ctx.check(fav && fav->color == "red" && fav->name == favName && fav->inbuilt, "inbuilt group recoloured");

    f.data().groups["some-uid"] = {id};
    f.act("delete", {{"id", id}});
    ctx.check(!f.group(id), "delete removes the user group");
    ctx.check(!f.data().groups.contains("some-uid"), "delete drops the group from waystone assignments");
    ctx.check(f.screen() == "manage_groups", "still on Manage Groups");
});

BS_REGISTER_SELFTEST("groups.hide_inbuilt", [](Context& ctx) {
    GroupsFixture f(ctx);
    bs::service::openScreen(ctx.player(), "manage_groups", json::object());
    f.act("hide", {{"id", "waystones:global"}, {"hidden", true}});
    auto g = f.group("waystones:global");
    ctx.check(g && g->hidden, "global hidden");
    bool modelHidden = false;
    auto model       = f.model();
    for (auto const& m : model["groups"]) {
        if (m.value("id", "") == "waystones:global") modelHidden = m.value("hidden", false);
    }
    ctx.check(modelHidden, "model updated with the hidden state");
    ctx.check(f.order().size() == 7, "hidden groups are still listed (callers skip them)");
    f.act("hide", {{"id", "waystones:global"}, {"hidden", false}});
    g = f.group("waystones:global");
    ctx.check(g && !g->hidden, "global unhidden");
});

BS_REGISTER_SELFTEST("groups.reorder", [](Context& ctx) {
    GroupsFixture f(ctx);
    auto id = f.createViaScreens("Zeta", "teams", "blue");
    ctx.require(!id.empty(), "group created");
    f.act("swap", {{"a", "waystones:favorites"}, {"b", "waystones:players"}});
    auto o = f.order();
    ctx.check(o[0] == "waystones:players" && o[1] == "waystones:favorites", "swap reorders");
    // Drag the user group up step by step (like the native screen does).
    for (int i = 0; i < 7; ++i) {
        auto cur = f.order();
        auto it  = std::find(cur.begin(), cur.end(), id);
        if (it == cur.begin()) break;
        f.act("swap", {{"a", *it}, {"b", *(it - 1)}});
    }
    ctx.check(f.order().front() == id, "dragged to the top");
    f.act("move", {{"id", id}, {"index", 99}});
    ctx.check(f.order().back() == id, "move to the bottom");
    auto list = bs::service::get<bs::service::IGroups>().groups(ctx.player());
    ctx.check(std::is_sorted(list.begin(), list.end(), [](auto const& a, auto const& b) { return a.sortIndex < b.sortIndex; }), "sort indices persisted");
});

BS_REGISTER_SELFTEST("groups.groups_of", [](Context& ctx) {
    GroupsFixture f(ctx);
    auto  id = f.createViaScreens("Homes", "player_homes", "gold");
    bs::core::WaystoneRecord rec;
    rec.name       = "Team Base";
    rec.pos        = ctx.testPos(1200);
    rec.visibility = bs::core::Visibility::Team;
    auto& r        = ctx.createWaystone(rec);
    f.data().groups[r.uid] = {"waystones:favorites", id, "waystones:user/deleted"};
    bs::core::WarpTarget t;
    t.id   = r.uid;
    t.kind = "waystone";
    t.pos  = r.pos;
    auto of  = bs::service::get<bs::service::IGroups>().groupsOf(ctx.player(), t);
    auto has = [&](std::string const& g) { return std::find(of.begin(), of.end(), g) != of.end(); };
    ctx.check(has("waystones:favorites") && has(id), "explicit groups");
    ctx.check(!has("waystones:user/deleted"), "deleted groups are dropped");
    ctx.check(has("waystones:teams"), "team waystones are in Teams");
    ctx.check(has(cg::dimensionGroup(r.pos.dim)), "dimension group");
    ctx.check(!has("waystones:global"), "not global");
    bs::core::WarpTarget pt;
    pt.id     = "player:abc";
    pt.kind   = "player";
    pt.global = true;
    auto pof  = bs::service::get<bs::service::IGroups>().groupsOf(ctx.player(), pt);
    ctx.check(std::find(pof.begin(), pof.end(), "waystones:players") != pof.end(), "players are in Players");
    ctx.check(std::find(pof.begin(), pof.end(), "waystones:global") != pof.end(), "global entries are in Global Waystones");
    ctx.check(std::none_of(pof.begin(), pof.end(), [](auto const& g) { return g.starts_with("waystones:dimension/"); }), "players have no dimension group");
});

BS_REGISTER_SELFTEST("groups.validation", [](Context& ctx) {
    GroupsFixture f(ctx);
    auto id = f.createViaScreens("Keep", "dungeons", "red");
    bs::service::openScreen(ctx.player(), "manage_groups", json::object());
    f.act("delete", {{"id", "waystones:favorites"}});
    ctx.check(f.group("waystones:favorites").has_value(), "inbuilt groups cannot be deleted");
    f.act("delete", {{"id", 42}});
    f.act("hide", {{"id", id}, {"hidden", true}});
    ctx.check(f.group(id) && !f.group(id)->hidden, "user groups cannot be hidden");
    f.act("swap", {{"a", id}, {"b", "waystones:user/unknown"}});
    ctx.check(f.order().back() == id, "swap with an unknown group is ignored");
    f.act("edit", {{"id", "waystones:user/unknown"}});
    ctx.check(f.screen() == "manage_groups", "editing an unknown group is ignored");
    auto before = f.order().size();
    f.act("create");
    f.act("save", {{"name", "Bad"}, {"icon", "../../etc"}, {"color", "white"}});
    ctx.check(f.order().size() == before, "invalid icon rejected");
    f.act("save", {{"name", "Bad"}, {"icon", "teams"}, {"color", "black"}});
    ctx.check(f.order().size() == before, "colour outside the 15 rejected");
    ctx.check(f.screen() == "edit_group", "edit screen stays open after a rejected save");
    f.act("save", {{"name", std::string(400, 'x')}, {"icon", "teams"}, {"color", "white"}});
    ctx.check(f.order().size() == before + 1, "valid save accepted");
    bool capped = false;
    for (auto const& g : bs::service::get<bs::service::IGroups>().groups(ctx.player())) {
        if (g.name.size() == cg::kMaxNameLength) capped = true;
    }
    ctx.check(capped, "long names are capped");
    // A stale action for another screen does nothing.
    bs::net::screens::close(ctx.player());
    f.act("delete", {{"id", id}});
    ctx.check(f.group(id).has_value(), "actions without an open screen are ignored");
});

BS_REGISTER_SELFTEST("groups.back", [](Context& ctx) {
    GroupsFixture f(ctx);
    // No source and nothing to go back to: Back closes.
    bs::service::openScreen(ctx.player(), "manage_groups", json::object());
    f.act("back");
    ctx.check(f.screen().empty(), "back without a source closes the screen");
    // Edit Group -> back returns to Manage Groups with the same args.
    bs::service::openScreen(ctx.player(), "edit_group", {{"group", ""}, {"from", "personal_settings"}, {"uid", "u1"}});
    ctx.require(f.screen() == "edit_group", "edit_group opener");
    f.act("back");
    ctx.require(f.screen() == "manage_groups", "back from Edit Group");
    auto args = bs::net::screens::current(ctx.player())->context["args"];
    ctx.check(args.value("from", "") == "personal_settings" && args.value("uid", "") == "u1", "return args survive the round trip");
    ctx.check(!args.contains("group"), "edit-only args dropped");
    ctx.check(std::count(f.opened.begin(), f.opened.end(), "manage_groups") >= 2, "screens sent to the client");
});
