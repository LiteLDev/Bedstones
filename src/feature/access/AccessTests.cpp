// Selftests of U9's permission and visibility rules (`/waystones selftest "access.*"`).
// Test positions: dx 900..949.

#include <algorithm>

#include "mc/server/SimulatedPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/GameType.h"

#include "feature/access/Access.h"
#include "feature/access/GeneralConfig.h"
#include "game/Waystones.h"
#include "mod/Config.h"
#include "selftest/SelfTest.h"
#include "service/Services.h"

namespace {

using bs::core::Origin;
using bs::core::Visibility;
using bs::core::WaystoneRecord;
using bs::feature::access::GeneralConfig;
using bs::selftest::Context;

constexpr char const* kStranger = "11111111-1111-4111-8111-111111111111";

/// Restores the `general` config when the test ends.
struct ConfigGuard {
    GeneralConfig saved = bs::config::get<GeneralConfig>();
    ~ConfigGuard() { bs::config::mut<GeneralConfig>() = saved; }
    GeneralConfig& operator*() { return bs::config::mut<GeneralConfig>(); }
    GeneralConfig* operator->() { return &bs::config::mut<GeneralConfig>(); }
};

bs::service::IPermissions& perms() { return bs::service::get<bs::service::IPermissions>(); }

void setCreative(Player& p, bool creative) { p.setPlayerGameType(creative ? GameType::Creative : GameType::Survival); }

WaystoneRecord waystone(Context& ctx, int dx, std::string name, std::string owner = {}) {
    WaystoneRecord r;
    r.name     = std::move(name);
    r.pos      = ctx.testPos(dx);
    r.style    = "waystone";
    r.origin   = Origin::Player;
    r.ownerUid = std::move(owner);
    return r;
}

std::string deniedKey(Player const& p, WaystoneRecord const& r) {
    auto d = perms().canEdit(p, r);
    return d ? d->key : "";
}

std::string list(std::vector<Visibility> const& v) {
    std::string out;
    for (auto x : v) out += std::string(bs::core::toString(x)) + ",";
    return out;
}

} // namespace

BS_REGISTER_SELFTEST("access.edit_owner", [](Context& ctx) {
    ConfigGuard cfg;
    *cfg     = GeneralConfig{};
    auto& p  = ctx.player();
    setCreative(p, false);
    auto& theirs = ctx.createWaystone(waystone(ctx, 900, "Theirs", kStranger));
    auto& mine   = ctx.createWaystone(waystone(ctx, 901, "Mine", bs::game::Waystones::playerKey(p)));
    auto& nobody = ctx.createWaystone(waystone(ctx, 902, "Nobody's"));
    ctx.check(deniedKey(p, theirs) == "chat.waystones.only_owner_can_edit", "a survival non-owner may not edit");
    ctx.check(deniedKey(p, mine).empty(), "the owner may edit");
    ctx.check(deniedKey(p, nobody).empty(), "an unowned player waystone is editable");
    cfg->restrictedWaystones = {};
    ctx.check(deniedKey(p, theirs).empty(), "without PLAYER in restrictedWaystones anyone may edit");
    cfg->restrictedWaystones = {"PLAYER"};
    setCreative(p, true);
    ctx.check(deniedKey(p, theirs).empty(), "creative (edit_all) bypasses the owner check");
    setCreative(p, false);
});

BS_REGISTER_SELFTEST("access.edit_origin", [](Context& ctx) {
    ConfigGuard cfg;
    *cfg    = GeneralConfig{};
    auto& p = ctx.player();
    setCreative(p, false);
    auto r   = waystone(ctx, 905, "Village");
    r.origin = Origin::Village;
    auto& v  = ctx.createWaystone(r);
    ctx.check(deniedKey(p, v).empty(), "village waystones are editable by default");
    cfg->restrictedWaystones = {"PLAYER", "VILLAGE"};
    ctx.check(deniedKey(p, v) == "chat.waystones.only_creative_can_edit", "a restricted origin needs creative");
    cfg->restrictedWaystones = {"player", "wilderness"};
    ctx.check(deniedKey(p, v).empty(), "other origins stay editable (values are case-insensitive)");
    cfg->restrictedWaystones = {"VILLAGE"};
    setCreative(p, true);
    ctx.check(deniedKey(p, v).empty(), "creative bypasses origin restrictions");
    setCreative(p, false);
});

BS_REGISTER_SELFTEST("access.edit_global", [](Context& ctx) {
    ConfigGuard cfg;
    *cfg    = GeneralConfig{};
    auto& p = ctx.player();
    setCreative(p, false);
    auto r       = waystone(ctx, 908, "Global", bs::game::Waystones::playerKey(p));
    r.visibility = Visibility::Global;
    auto& g      = ctx.createWaystone(r);
    ctx.check(deniedKey(p, g) == "chat.waystones.only_creative_can_edit_global", "a global waystone needs global management");
    cfg->allowEveryoneToManageGlobalWaystones = true;
    ctx.check(deniedKey(p, g).empty(), "allowEveryoneToManageGlobalWaystones lets the owner edit");
    cfg->allowEveryoneToManageGlobalWaystones = false;
    cfg->defaultVisibility                    = "GLOBAL";
    ctx.check(deniedKey(p, g).empty(), "defaultVisibility GLOBAL lets everyone manage global waystones");
    cfg->defaultVisibility = "ACTIVATION";
    setCreative(p, true);
    ctx.check(deniedKey(p, g).empty(), "creative may edit global waystones");
    setCreative(p, false);
});

BS_REGISTER_SELFTEST("access.manage_global", [](Context& ctx) {
    ConfigGuard cfg;
    *cfg    = GeneralConfig{};
    auto& p = ctx.player();
    setCreative(p, false);
    ctx.check(!perms().canManageGlobal(p), "survival players can't manage global waystones by default");
    setCreative(p, true);
    ctx.check(perms().canManageGlobal(p), "creative players can");
    setCreative(p, false);
    cfg->allowEveryoneToManageGlobalWaystones = true;
    ctx.check(perms().canManageGlobal(p), "allowEveryoneToManageGlobalWaystones");
    cfg->allowEveryoneToManageGlobalWaystones = false;
    cfg->defaultVisibility                    = "GLOBAL";
    ctx.check(perms().canManageGlobal(p), "defaultVisibility GLOBAL");
    cfg->defaultVisibility   = "ACTIVATION";
    cfg->allowedVisibilities = {"GLOBAL"};
    ctx.check(perms().canManageGlobal(p), "deprecated allowedVisibilities GLOBAL");
});

BS_REGISTER_SELFTEST("access.visibility_options", [](Context& ctx) {
    ConfigGuard cfg;
    *cfg    = GeneralConfig{};
    auto& p = ctx.player();
    setCreative(p, false);
    auto& w = ctx.createWaystone(waystone(ctx, 910, "Options"));
    ctx.check(list(perms().visibilityOptions(p, w)) == "activation,team,", "survival: " + list(perms().visibilityOptions(p, w)));
    setCreative(p, true);
    ctx.check(list(perms().visibilityOptions(p, w)) == "activation,team,global,", "creative: " + list(perms().visibilityOptions(p, w)));
    setCreative(p, false);
    cfg->defaultVisibility = "TEAM";
    ctx.check(perms().defaultVisibility() == Visibility::Team, "defaultVisibility follows the config");
    ctx.check(list(perms().visibilityOptions(p, w)) == "team,activation,", "default TEAM first: " + list(perms().visibilityOptions(p, w)));
    cfg->defaultVisibility = "GLOBAL";
    ctx.check(list(perms().visibilityOptions(p, w)) == "global,activation,team,", "default GLOBAL first: " + list(perms().visibilityOptions(p, w)));
    cfg->defaultVisibility = "bogus";
    ctx.check(perms().defaultVisibility() == Visibility::Activation, "invalid default falls back to ACTIVATION");
    auto s       = waystone(ctx, 911, "Share");
    s.type       = bs::core::types::sharestone("red");
    s.visibility = Visibility::Sharestone;
    ctx.check(list(perms().visibilityOptions(p, ctx.createWaystone(s))) == "sharestone,", "sharestones: fixed");
    auto plate       = waystone(ctx, 912, "Plate");
    plate.type       = std::string(bs::core::types::kWarpPlate);
    plate.visibility = Visibility::ShardOnly;
    ctx.check(list(perms().visibilityOptions(p, ctx.createWaystone(plate))) == "shard_only,", "warp plates: fixed");
});

BS_REGISTER_SELFTEST("access.see_activation", [](Context& ctx) {
    auto& ws = bs::game::Waystones::get();
    auto& p  = ctx.player();
    auto& r  = ctx.createWaystone(waystone(ctx, 915, "Activation", kStranger));
    ctx.check(!perms().canSee(p, r), "not visible before activation");
    ws.activate(p, r.uid);
    ctx.check(perms().canSee(p, r), "visible once activated");
    ws.forget(p, r.uid);
    auto& mine = ctx.createWaystone(waystone(ctx, 916, "Own", bs::game::Waystones::playerKey(p)));
    ctx.check(perms().canSee(p, mine), "the owner always sees it");
});

BS_REGISTER_SELFTEST("access.see_global", [](Context& ctx) {
    auto& p      = ctx.player();
    auto  r      = waystone(ctx, 918, "Everyone", kStranger);
    r.visibility = Visibility::Global;
    auto& g      = ctx.createWaystone(r);
    ctx.check(perms().canSee(p, g), "global waystones are visible to everyone");
    auto list = bs::feature::access::globalWaystones();
    ctx.check(std::find(list.begin(), list.end(), &g) != list.end(), "listed in the global index");
    g.transient = true;
    bs::game::Waystones::get().markDirty(g.uid);
    ctx.check(!perms().canSee(p, g), "transient global waystones are hidden");
    list = bs::feature::access::globalWaystones();
    ctx.check(std::find(list.begin(), list.end(), &g) == list.end(), "transient: dropped from the index");
    g.transient = false;
    g.visibility = Visibility::Activation;
    bs::game::Waystones::get().markDirty(g.uid);
    list = bs::feature::access::globalWaystones();
    ctx.check(std::find(list.begin(), list.end(), &g) == list.end(), "no longer global: dropped from the index");
    auto wild       = waystone(ctx, 919, "");
    wild.origin     = Origin::Wilderness;
    wild.visibility = Visibility::Global;
    auto& w         = ctx.createWaystone(wild);
    ctx.check(!perms().canSee(p, w), "generated but unnamed waystones are hidden");
    w.name = "Named";
    bs::game::Waystones::get().markDirty(w.uid);
    ctx.check(perms().canSee(p, w), "visible once named");
});

BS_REGISTER_SELFTEST("access.see_team", [](Context& ctx) {
    auto& owner  = ctx.player();
    auto& viewer = ctx.spawnPlayer("bs_u9_viewer");
    auto  r      = waystone(ctx, 922, "Team", bs::game::Waystones::playerKey(owner));
    r.visibility = Visibility::Team;
    auto& t      = ctx.createWaystone(r);
    ctx.check(!perms().canSee(viewer, t), "no team: not visible");
    owner.addTag("team:bs_u9");
    viewer.addTag("team:bs_u9");
    ctx.check(bs::feature::access::sameTeam(viewer, t.ownerUid), "shared team tag = same team");
    ctx.check(perms().canSee(viewer, t), "same team: visible");
    auto list = bs::feature::access::teamWaystones(viewer);
    ctx.check(std::find(list.begin(), list.end(), &t) != list.end(), "listed in the team index");
    viewer.removeTag("team:bs_u9");
    viewer.addTag("team:other");
    ctx.check(!perms().canSee(viewer, t), "other team: not visible");
    list = bs::feature::access::teamWaystones(viewer);
    ctx.check(std::find(list.begin(), list.end(), &t) == list.end(), "other team: not in the team list");
    ctx.check(perms().canSee(owner, t), "the owner sees it");
    viewer.removeTag("team:other");
    owner.removeTag("team:bs_u9");
    (void)bs::feature::access::teamsOf(viewer); // drop the test teams from the cache
    (void)bs::feature::access::teamsOf(owner);
});

BS_REGISTER_SELFTEST("access.see_fixed", [](Context& ctx) {
    auto& p      = ctx.player();
    auto  s      = waystone(ctx, 925, "Share");
    s.type       = bs::core::types::sharestone("blue");
    s.visibility = Visibility::Sharestone;
    ctx.check(perms().canSee(p, ctx.createWaystone(s)), "sharestones are visible to everyone");
    auto plate       = waystone(ctx, 926, "Plate");
    plate.type       = std::string(bs::core::types::kWarpPlate);
    plate.visibility = Visibility::ShardOnly;
    ctx.check(!perms().canSee(p, ctx.createWaystone(plate)), "shard-only plates are never listed");
});
