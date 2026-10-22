// Selftests of U11 (`/waystones selftest "manage.*"`), dx range 1100..1199. They drive the
// screens like a client would: the simulated player is marked as a companion, packets go to a
// test sink and actions run through net::screens::simulateAction.

#include <algorithm>
#include <cmath>

#include "mc/world/actor/player/Player.h"
#include "mc/world/level/GameType.h"

#include "feature/manage/Manage.h"
#include "game/Ids.h"
#include "game/Waystones.h"
#include "net/Companion.h"
#include "net/Screens.h"
#include "selftest/SelfTest.h"
#include "service/ScreenOpeners.h"
#include "service/Services.h"

namespace bs::feature::manage {

namespace {

using selftest::Context;
namespace screens = net::screens;

/// Marks the test player as a companion for the test's lifetime and counts screen packets.
struct CompanionScope {
    Player& player;
    int     opened = 0, updated = 0;
    explicit CompanionScope(Player& p) : player(p) {
        net::setTestSink([this](Player&, std::string_view, nlohmann::json const& body) {
            // ScreenOpen = {session, id, model}, ScreenUpdate = {session, model}.
            if (!body.contains("model")) return;
            if (body.contains("id")) ++opened;
            else ++updated;
        });
        net::setCompanion(p, net::CompanionInfo{"test", {"native_screen"}});
    }
    ~CompanionScope() {
        screens::close(player);
        net::setTestSink(nullptr);
        net::setCompanion(player, std::nullopt);
    }
};

core::WarpSource commandSource() {
    core::WarpSource s;
    s.kind = core::WarpSourceKind::Command;
    return s;
}

/// Creates `names.size()` activated waystones (in that manual order) for the test player.
std::vector<std::string> activatedWaystones(Context& ctx, int dx, std::vector<std::string> const& names) {
    auto&                    ws = game::Waystones::get();
    std::vector<std::string> uids;
    for (std::size_t i = 0; i < names.size(); ++i) {
        core::WaystoneRecord r;
        r.name   = names[i];
        r.pos    = ctx.testPos(dx + static_cast<int>(i));
        r.style  = "waystone";
        r.origin = core::Origin::Player;
        auto uid = ctx.createWaystone(r).uid;
        ws.activate(ctx.player(), uid);
        uids.push_back(uid);
    }
    return uids;
}

/// The ids among `of` in the order the manage screen lists them.
std::vector<std::string> listedOrder(Player& p, std::vector<std::string> const& of, core::WarpSource const& src = commandSource()) {
    std::vector<std::string> out;
    for (auto const& e : entries(p, src)) {
        if (std::find(of.begin(), of.end(), e.target.id) != of.end()) out.push_back(e.target.id);
    }
    return out;
}

nlohmann::json const* modelRow(Player& p, std::string const& id) {
    auto* s = screens::current(p, kManageScreen);
    if (!s) return nullptr;
    for (auto const& row : s->model["rows"]) {
        if (row.value("id", std::string{}) == id) return &row;
    }
    return nullptr;
}

bool isListed(Player& p, std::string const& id, core::WarpSource const& src = commandSource()) {
    return !listedOrder(p, {id}, src).empty();
}

void setCreative(Player& p, bool creative) { p.setPlayerGameType(creative ? GameType::Creative : GameType::Survival); }

} // namespace

} // namespace bs::feature::manage

using namespace bs::feature::manage;
using bs::selftest::Context;

BS_REGISTER_SELFTEST("manage.open", [](Context& ctx) {
    auto&          p = ctx.player();
    CompanionScope scope(p);
    auto           ids = activatedWaystones(ctx, 1100, {"M Open A", "M Open B"});
    openManage(p, commandSource());
    auto* s = screens::current(p, kManageScreen);
    ctx.require(s != nullptr, "manage screen is open");
    ctx.check(scope.opened == 1, "ScreenOpen packet sent");
    ctx.check(s->model.value("title", std::string{}) == "Manage Waystones", "title");
    auto* a = modelRow(p, ids[0]);
    ctx.require(a != nullptr, "activated waystones are listed");
    ctx.check(a->value("canDelete", false) && !a->value("canHide", true), "an activated waystone has the delete X");
    ctx.check(listedOrder(p, ids) == ids, "listed in sorting-index order");
    ctx.check(bs::service::openScreen(p, "manage", {{"source", commandSource().toJson()}}), "opener registered");
    ctx.check(bs::service::hasScreen("personal_settings"), "personal_settings opener registered");
});

BS_REGISTER_SELFTEST("manage.swap", [](Context& ctx) {
    auto&          p = ctx.player();
    CompanionScope scope(p);
    auto           ids = activatedWaystones(ctx, 1110, {"M Swap A", "M Swap B", "M Swap C"});
    openManage(p, commandSource());
    screens::simulateAction(p, "swap", {{"from", ids[0]}, {"to", ids[1]}});
    ctx.check(listedOrder(p, ids) == std::vector{ids[1], ids[0], ids[2]}, "first drag step swaps A and B");
    screens::simulateAction(p, "swap", {{"from", ids[0]}, {"to", ids[2]}});
    ctx.check(listedOrder(p, ids) == std::vector{ids[1], ids[2], ids[0]}, "second drag step moves A to the bottom");
    ctx.check(scope.updated >= 2, "the screen model is refreshed after each step");
    screens::simulateAction(p, "swap", {{"from", ids[0]}, {"to", "not-listed"}});
    ctx.check(listedOrder(p, ids) == std::vector{ids[1], ids[2], ids[0]}, "swap with an unlisted id is refused");
});

BS_REGISTER_SELFTEST("manage.move_top_bottom", [](Context& ctx) {
    auto&          p = ctx.player();
    CompanionScope scope(p);
    auto           ids = activatedWaystones(ctx, 1120, {"M Move A", "M Move B", "M Move C"});
    openManage(p, commandSource());
    screens::simulateAction(p, "move_top", {{"id", ids[2]}});
    ctx.check(listedOrder(p, ids) == std::vector{ids[2], ids[0], ids[1]}, "double-click moves to the top");
    screens::simulateAction(p, "move_bottom", {{"id", ids[2]}});
    ctx.check(listedOrder(p, ids) == std::vector{ids[0], ids[1], ids[2]}, "shift-double-click moves to the bottom");
});

BS_REGISTER_SELFTEST("manage.delete_forget", [](Context& ctx) {
    auto&          p = ctx.player();
    CompanionScope scope(p);
    setCreative(p, false);
    auto ids = activatedWaystones(ctx, 1130, {"M Forget A", "M Forget B"});
    openManage(p, commandSource());
    screens::simulateAction(p, "delete", {{"id", ids[0]}});
    auto& ws = bs::game::Waystones::get();
    ctx.check(!ws.isActivated(p, ids[0]), "survival delete forgets the waystone");
    ctx.check(ws.find(ids[0]) != nullptr, "the record itself stays");
    ctx.check(modelRow(p, ids[0]) == nullptr && modelRow(p, ids[1]) != nullptr, "the row disappears from the model");
});

BS_REGISTER_SELFTEST("manage.delete_sharestone", [](Context& ctx) {
    auto&          p = ctx.player();
    CompanionScope scope(p);
    auto&          ws = bs::game::Waystones::get();
    auto make = [&](int dx, std::string name) {
        bs::core::WaystoneRecord r;
        r.type       = bs::core::types::sharestone("red");
        r.style      = "red_sharestone";
        r.name       = std::move(name);
        r.pos        = ctx.testPos(dx);
        r.visibility = bs::core::Visibility::Sharestone;
        return ctx.createWaystone(r).uid;
    };
    auto from = make(1140, "M Share From"), target = make(1141, "M Share Target");
    bs::core::WarpSource src;
    src.kind    = bs::core::WarpSourceKind::Sharestone;
    src.fromUid = from;
    src.fromPos = ctx.testPos(1140);
    setCreative(p, false);
    openManage(p, src);
    auto* row = modelRow(p, target);
    ctx.require(row != nullptr, "sharestones of the same color are listed");
    ctx.check(!row->value("canDelete", true) && row->value("canHide", false), "survival: eye toggle instead of delete");
    ctx.check(row->value("everyone", false), "sharestone delete warns about everyone");
    screens::simulateAction(p, "delete", {{"id", target}});
    ctx.check(ws.find(target) != nullptr, "survival delete of a sharestone is refused");
    setCreative(p, true);
    ctx.require(p.isCreative(), "creative mode applied");
    openManage(p, src);
    screens::simulateAction(p, "delete", {{"id", target}});
    ctx.check(ws.find(target) == nullptr, "creative delete purges the sharestone");
    setCreative(p, false);
});

BS_REGISTER_SELFTEST("manage.delete_global", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto&          p = ctx.player();
    CompanionScope scope(p);
    auto&          ws = bs::game::Waystones::get();
    // Next to the player, so the chunk is loaded and the block check can tell "gone" apart.
    auto feet = p.getFeetPos();
    auto near = [&](int dx) {
        return bs::core::GlobalPos{static_cast<int>(p.getDimensionId()), static_cast<int>(std::floor(feet.x)) + dx,
                                   static_cast<int>(std::floor(feet.y)), static_cast<int>(std::floor(feet.z)) + 3};
    };
    auto make = [&](bs::core::GlobalPos pos, std::string name) {
        bs::core::WaystoneRecord r;
        r.name       = std::move(name);
        r.pos        = pos;
        r.style      = "waystone";
        r.visibility = bs::core::Visibility::Global;
        return ctx.createWaystone(r).uid;
    };
    auto gone = make(near(-2), "M Global Gone");
    auto kept = make(near(2), "M Global Kept");
    // A freshly spawned simulated player needs a moment until its chunks are loaded.
    for (int i = 0; i < 100 && !bs::feature::manage::blockExists(near(-2)); ++i) co_await ctx.ticks(2);
    ctx.require(bs::feature::manage::blockExists(near(-2)) == std::optional<bool>{false}, "the test area is loaded and empty");
    bool hasBlock = ctx.setBlock(near(2), std::string(bs::ids::kNamespace) + ":waystone", {});
    setCreative(p, false);
    openManage(p, commandSource());
    auto* row = modelRow(p, gone);
    ctx.require(row != nullptr, "global waystones are listed");
    ctx.check(!row->value("canDelete", true) && row->value("canHide", false), "survival: global waystones can only be hidden");
    screens::simulateAction(p, "delete", {{"id", gone}});
    ctx.check(ws.find(gone) != nullptr && ws.find(gone)->isGlobal(), "survival delete of a global waystone is refused");

    setCreative(p, true);
    openManage(p, commandSource());
    screens::simulateAction(p, "delete", {{"id", gone}});
    ctx.check(ws.find(gone) == nullptr, "creative delete of a global waystone whose block is gone purges it");
    if (hasBlock) {
        screens::simulateAction(p, "delete", {{"id", kept}});
        auto* r = ws.find(kept);
        ctx.check(r && r->visibility == bs::core::Visibility::Activation, "creative delete demotes a global waystone to activation");
        ctx.check(!isListed(p, kept), "the demoted waystone leaves the list");
    } else {
        ctx.info("bedstones:waystone is not registered (U1 absent): demote-with-block case skipped");
    }
    setCreative(p, false);
});

BS_REGISTER_SELFTEST("manage.validation", [](Context& ctx) {
    auto&          p = ctx.player();
    CompanionScope scope(p);
    auto&          ws = bs::game::Waystones::get();
    setCreative(p, false);
    // Someone else's team waystone that this player never activated is not even listed.
    bs::core::WaystoneRecord team;
    team.name       = "M Team";
    team.pos        = ctx.testPos(1160);
    team.visibility = bs::core::Visibility::Team;
    team.ownerUid   = "someone-else";
    auto teamUid    = ctx.createWaystone(team).uid;
    auto ids        = activatedWaystones(ctx, 1162, {"M Valid A"});
    openManage(p, commandSource());
    screens::simulateAction(p, "delete", {{"id", teamUid}});
    ctx.check(ws.find(teamUid) != nullptr, "delete of an unlisted waystone is refused");
    screens::simulateAction(p, "delete", {{"id", "no-such-uid"}});
    screens::simulateAction(p, "set_hidden", {{"id", ids[0]}, {"hidden", true}});
    ctx.check(!ws.playerData(p).hidden.contains(ids[0]), "hiding a deletable waystone is refused");
    ctx.check(!openPersonal(p, teamUid, commandSource()), "personal settings of an unlisted waystone are refused");
    ctx.check(screens::current(p, kManageScreen) != nullptr, "the manage screen stays open after refused actions");
    // The team waystone becomes deletable once activated.
    ws.activate(p, teamUid);
    openManage(p, commandSource());
    auto* row = modelRow(p, teamUid);
    ctx.check(row && row->value("canDelete", false), "an activated team waystone can be deleted");
});

BS_REGISTER_SELFTEST("manage.hide_show", [](Context& ctx) {
    auto&          p = ctx.player();
    CompanionScope scope(p);
    auto&          ws = bs::game::Waystones::get();
    setCreative(p, false);
    bs::core::WaystoneRecord g;
    g.name       = "M Hide Global";
    g.pos        = ctx.testPos(1170);
    g.visibility = bs::core::Visibility::Global;
    auto uid     = ctx.createWaystone(g).uid;
    openManage(p, commandSource());
    screens::simulateAction(p, "set_hidden", {{"id", uid}, {"hidden", true}});
    ctx.check(ws.playerData(p).hidden.contains(uid), "hide adds to the hidden set");
    auto* row = modelRow(p, uid);
    ctx.check(row && row->value("hidden", false), "hidden entries stay listed (eye toggle)");
    screens::simulateAction(p, "set_hidden", {{"id", uid}, {"hidden", false}});
    ctx.check(!ws.playerData(p).hidden.contains(uid), "show removes it again");
});

BS_REGISTER_SELFTEST("manage.alias", [](Context& ctx) {
    auto&          p = ctx.player();
    CompanionScope scope(p);
    auto&          ws  = bs::game::Waystones::get();
    auto           ids = activatedWaystones(ctx, 1175, {"M Alias"});
    openManage(p, commandSource());
    screens::simulateAction(p, "personal_settings", {{"id", ids[0]}});
    auto* s = screens::current(p, kPersonalScreen);
    ctx.require(s != nullptr, "personal settings open from the edit icon");
    ctx.check(s->model.value("name", std::string{}) == "M Alias", "title is the waystone name");
    ctx.check(!s->model.value("canEdit", true), "no Edit Waystone button when far away");
    screens::simulateAction(p, "save", {{"alias", "My Spot"}, {"group", ""}, {"favorite", false}});
    ctx.check(ws.playerData(p).aliases[ids[0]] == "My Spot", "alias saved");
    ctx.check(screens::current(p, kManageScreen) != nullptr, "save returns to Manage Waystones");
    auto* row = modelRow(p, ids[0]);
    ctx.check(row && row->value("name", std::string{}) == "My Spot", "the list shows the alias");
    openPersonal(p, ids[0], commandSource());
    screens::simulateAction(p, "save", {{"alias", "   "}, {"group", ""}, {"favorite", false}});
    ctx.check(!ws.playerData(p).aliases.contains(ids[0]), "a blank alias removes it");
    openPersonal(p, ids[0], commandSource());
    screens::simulateAction(p, "save", {{"alias", std::string(300, 'x')}, {"group", ""}, {"favorite", false}});
    ctx.check(ws.playerData(p).aliases[ids[0]].size() == 128, "alias clipped to 128 characters");
});

BS_REGISTER_SELFTEST("manage.favorite", [](Context& ctx) {
    auto&          p = ctx.player();
    CompanionScope scope(p);
    auto&          ws  = bs::game::Waystones::get();
    auto           ids = activatedWaystones(ctx, 1180, {"M Fav"});
    auto has = [&](std::string const& g) {
        auto& d  = ws.playerData(p);
        auto  it = d.groups.find(ids[0]);
        return it != d.groups.end() && std::find(it->second.begin(), it->second.end(), g) != it->second.end();
    };
    std::string fav(bs::service::groups::kFavorites);
    openPersonal(p, ids[0], commandSource());
    ctx.check(!screens::current(p)->model.value("favorite", true), "not a favorite at first");
    screens::simulateAction(p, "save", {{"alias", ""}, {"group", ""}, {"favorite", true}});
    ctx.check(has(fav), "checking Favorite adds the favorites group");
    openPersonal(p, ids[0], commandSource());
    ctx.check(screens::current(p)->model.value("favorite", false), "the checkbox reflects it");
    screens::simulateAction(p, "save", {{"alias", ""}, {"group", ""}, {"favorite", false}});
    ctx.check(!has(fav) && !ws.playerData(p).groups.contains(ids[0]), "unchecking removes it");
});

BS_REGISTER_SELFTEST("manage.group_cycle", [](Context& ctx) {
    auto&          p = ctx.player();
    CompanionScope scope(p);
    auto&          ws  = bs::game::Waystones::get();
    auto           ids = activatedWaystones(ctx, 1185, {"M Group"});
    auto&          d   = ws.playerData(p);
    d.groupRegistry.push_back({"waystones:user/manage-test-1", "Mines", "resource_sites", "red", false, false, 0});
    d.groupRegistry.push_back({"waystones:user/manage-test-2", "Homes", "player_homes", "green", false, false, 1});
    openPersonal(p, ids[0], commandSource());
    auto& model = screens::current(p)->model;
    std::vector<std::string> userIds;
    for (auto const& g : model["groups"]) userIds.push_back(g.value("id", std::string{}));
    ctx.require(userIds.size() == 2, "only user groups are offered");
    ctx.check(model.value("group", std::string{"x"}).empty(), "No Group at first");
    // Left-click: next group (+ favorite, which must survive the group change).
    auto next = bs::core::manage::cycleGroup(userIds, "", 1);
    screens::simulateAction(p, "save", {{"alias", ""}, {"group", next}, {"favorite", true}});
    auto const& stored = d.groups[ids[0]];
    ctx.check(
        stored.size() == 2 && std::find(stored.begin(), stored.end(), next) != stored.end()
            && std::find(stored.begin(), stored.end(), std::string(bs::service::groups::kFavorites)) != stored.end(),
        "group + favorite stored"
    );
    openPersonal(p, ids[0], commandSource());
    ctx.check(screens::current(p)->model.value("group", std::string{}) == next, "the cycle button shows the stored group");
    // Right-click from the first group: previous = No Group.
    auto prev = bs::core::manage::cycleGroup(userIds, next, -1);
    ctx.check(prev.empty(), "right-click cycles back to No Group");
    auto second = bs::core::manage::cycleGroup(userIds, next, 1);
    screens::simulateAction(p, "save", {{"alias", ""}, {"group", second}, {"favorite", true}});
    ctx.check(bs::core::manage::userGroupOf(d.groups[ids[0]], userIds) == second, "one user group at a time");
    openPersonal(p, ids[0], commandSource());
    screens::simulateAction(p, "save", {{"alias", ""}, {"group", "waystones:user/forged"}, {"favorite", false}});
    ctx.check(bs::core::manage::userGroupOf(d.groups[ids[0]], userIds) == second, "an unknown group is refused");
    std::erase_if(d.groupRegistry, [](auto const& g) { return g.id.starts_with("waystones:user/manage-test-"); });
});

BS_REGISTER_SELFTEST("manage.edit_link", [](Context& ctx) {
    auto&          p = ctx.player();
    CompanionScope scope(p);
    auto           ids = activatedWaystones(ctx, 1190, {"M Edit Far"});
    openPersonal(p, ids[0], commandSource());
    screens::simulateAction(p, "edit", {{"alias", "Far"}, {"group", ""}, {"favorite", false}});
    ctx.check(screens::current(p, kPersonalScreen) != nullptr, "edit is refused beyond 4 blocks (screen stays)");
    ctx.check(bs::game::Waystones::get().playerData(p).aliases[ids[0]] == "Far", "the settings were saved anyway");
    // Next to the player: the Edit Waystone button is offered.
    auto feet = p.getFeetPos();
    bs::core::WaystoneRecord near;
    near.name = "M Edit Near";
    near.pos  = {static_cast<int>(p.getDimensionId()), static_cast<int>(std::floor(feet.x)) + 1, static_cast<int>(std::floor(feet.y)),
                 static_cast<int>(std::floor(feet.z))};
    auto uid = ctx.createWaystone(near).uid;
    bs::game::Waystones::get().activate(p, uid);
    openPersonal(p, uid, commandSource());
    ctx.check(screens::current(p)->model.value("canEdit", false), "Edit Waystone offered within 4 blocks");
    screens::simulateAction(p, "back");
    ctx.check(screens::current(p, kManageScreen) != nullptr, "back returns to manage");
});
