// Selftests of the foundation: registry, player data, screens protocol and the default seams.

#include <cmath>
#include <memory>
#include <utility>

#include "mc/world/actor/player/Player.h"

#include "game/Waystones.h"
#include "net/Companion.h"
#include "net/Messages.h"
#include "net/Screens.h"
#include "selftest/SelfTest.h"
#include "service/Services.h"

namespace {

using bs::selftest::Context;

bs::core::WaystoneRecord sampleRecord(Context& ctx, int dx, std::string name) {
    bs::core::WaystoneRecord r;
    r.name   = std::move(name);
    r.pos    = ctx.testPos(dx);
    r.style  = "waystone";
    r.origin = bs::core::Origin::Player;
    return r;
}

/// The default_teleport / warp_events tests exercise the foundation's default ITeleport even when
/// a unit (U7) replaced it: its stricter pipeline needs real blocks and held items.
struct DefaultTeleportScope {
    std::unique_ptr<bs::service::ITeleport> saved = std::exchange(
        bs::service::detail::slot<bs::service::ITeleport>(),
        bs::service::detail::makeDefault<bs::service::ITeleport>()
    );
    ~DefaultTeleportScope() { bs::service::detail::slot<bs::service::ITeleport>() = std::move(saved); }
};

} // namespace

BS_REGISTER_SELFTEST("foundation.records", [](Context& ctx) {
    auto& ws  = bs::game::Waystones::get();
    auto& rec = ctx.createWaystone(sampleRecord(ctx, 40, "Alpha"));
    ctx.require(!rec.uid.empty(), "record has a uid");
    ctx.check(ws.find(rec.uid) == &rec, "find by uid");
    ctx.check(ws.findAt(rec.pos) == &rec, "find by position");
    int  updates  = 0;
    auto uid      = rec.uid;
    int  listener = ws.onChanged([&updates, uid](bs::game::WaystoneChange c, bs::core::WaystoneRecord const& r) {
        if (c == bs::game::WaystoneChange::Updated && r.uid == uid) ++updates;
    });
    rec.name = "Beta";
    ws.markDirty(uid);
    ws.removeListener(listener);
    ctx.check(updates == 1, "markDirty notifies listeners");
    auto moved = ctx.testPos(41);
    ws.move(uid, moved);
    ctx.check(ws.findAt(moved) != nullptr && ws.findAt(ctx.testPos(40)) == nullptr, "move re-indexes the position");
});

BS_REGISTER_SELFTEST("foundation.player_data", [](Context& ctx) {
    auto& ws  = bs::game::Waystones::get();
    auto& p   = ctx.player();
    auto& rec = ctx.createWaystone(sampleRecord(ctx, 42, "Home"));
    ctx.check(!ws.isActivated(p, rec.uid), "not activated at first");
    ctx.check(ws.activate(p, rec.uid), "activate");
    ctx.check(!ws.activate(p, rec.uid), "activate twice is a no-op");
    ctx.check(ws.isActivated(p, rec.uid), "activated");
    ws.playerData(p).aliases[rec.uid] = "My Home";
    ctx.check(ws.playerData(p).effectiveName(rec.uid, rec.name) == "My Home", "alias wins");
    ctx.check(ws.forget(p, rec.uid), "forget");
    ctx.check(!ws.isActivated(p, rec.uid), "forgotten");
    ws.activate(p, rec.uid);
    auto uid = rec.uid;
    ws.remove(uid);
    ctx.check(!ws.playerData(p).isActivated(uid), "removed waystones are pruned from player data");
});

BS_REGISTER_SELFTEST("foundation.screens", [](Context& ctx) {
    auto& p = ctx.player();
    std::vector<std::string> seen;
    bs::net::setTestSink([&seen](Player&, std::string_view packet, nlohmann::json const& body) {
        seen.push_back(std::string(packet) + ":" + body.value("id", body.value("channel", std::string{})));
    });
    bs::net::setCompanion(p, bs::net::CompanionInfo{"test", {"native_screen"}});
    std::string lastAction;
    auto        session = bs::net::screens::open(p, "foundation_test", {{"value", 1}}, {{"secret", 2}});
    ctx.check(session != 0, "screen opened for a companion");
    auto* s = bs::net::screens::current(p, "foundation_test");
    ctx.require(s != nullptr, "screen is current");
    ctx.check(s->context.value("secret", 0) == 2, "server context kept");
    bs::net::screens::update(p, {{"value", 3}});
    ctx.check(bs::net::screens::current(p)->model.value("value", 0) == 3, "model updated");
    bs::net::sendMessage(p, "foundation.ping", {{"x", 1}});
    bs::net::screens::simulateAction(p, "close");
    ctx.check(bs::net::screens::current(p) == nullptr, "close action closes the session");
    bool sawOpen = false, sawMsg = false;
    for (auto const& e : seen) {
        if (e.find("foundation_test") != std::string::npos) sawOpen = true;
        if (e.find("foundation.ping") != std::string::npos) sawMsg = true;
    }
    ctx.check(sawOpen, "ScreenOpen packet sent");
    ctx.check(sawMsg, "ServerMessage packet sent");
    bs::net::setTestSink(nullptr);
    bs::net::setCompanion(p, std::nullopt);
});

BS_REGISTER_SELFTEST("foundation.default_targets", [](Context& ctx) {
    auto& ws   = bs::game::Waystones::get();
    auto& p    = ctx.player();
    auto& a    = ctx.createWaystone(sampleRecord(ctx, 44, "A"));
    auto& b    = ctx.createWaystone(sampleRecord(ctx, 46, "B"));
    auto  aUid = a.uid, bUid = b.uid;
    ws.activate(p, aUid);
    ws.activate(p, bUid);
    bs::core::WarpSource src;
    src.kind    = bs::core::WarpSourceKind::Waystone;
    src.fromUid = aUid;
    auto list   = bs::service::get<bs::service::ITargets>().list(p, src);
    bool hasA = false, hasB = false, aCurrent = false;
    for (auto const& t : list) {
        if (t.id == aUid) hasA = true, aCurrent = t.current;
        if (t.id == bUid) hasB = true;
    }
    ctx.check(hasA && hasB, "activated waystones are listed");
    ctx.check(aCurrent, "the source waystone is flagged current");
});

BS_REGISTER_SELFTEST("foundation.default_teleport", [](Context& ctx) -> ll::coro::CoroTask<> {
    DefaultTeleportScope useDefault;
    auto& p   = ctx.player();
    auto  rec = sampleRecord(ctx, 48, "Target");
    rec.facing = 2; // north: lands one block north
    auto& r    = ctx.createWaystone(rec);
    bs::service::WarpRequest req;
    req.source.kind = bs::core::WarpSourceKind::Command;
    req.targetId    = r.uid;
    auto res        = bs::service::get<bs::service::ITeleport>().warp(p, req);
    ctx.check(res.ok, "warp succeeded");
    co_await ctx.ticks(2);
    auto pos = p.getPosition();
    ctx.check(std::abs(pos.x - (static_cast<float>(r.pos.x) + 0.5f)) < 1.f, "x in front of the waystone");
    ctx.check(std::abs(pos.z - (static_cast<float>(r.pos.z - 1) + 0.5f)) < 1.f, "z in front of the waystone (north)");
});

#include "game/BlockContainer.h"
#include "service/ScreenOpeners.h"

BS_REGISTER_SELFTEST("foundation.container", [](Context& ctx) {
    auto& rec = ctx.createWaystone(sampleRecord(ctx, 50, "Box"));
    auto  c   = bs::game::containerOf(rec);
    ctx.check(!c[0] && !c[4], "empty container by default");
    c[0] = bs::game::SlotItem{"bedstones:attuned_shard", 1, 0, {{"bs_attunement", "abc"}}};
    c[2] = bs::game::SlotItem{"minecraft:diamond", 3, 0, {}};
    bs::game::setContainer(rec, c);
    auto back = bs::game::containerOf(rec);
    ctx.check(back[0] && back[0]->user.at("bs_attunement") == "abc", "shard slot keeps its user data");
    ctx.check(back[2] && back[2]->count == 3, "modifier slot count");
    ctx.check(!back[1], "untouched slot stays empty");
});

BS_REGISTER_SELFTEST("foundation.groups", [](Context& ctx) {
    auto& p      = ctx.player();
    auto  groups = bs::service::get<bs::service::IGroups>().groups(p);
    ctx.check(groups.size() >= 7, "inbuilt groups present");
    ctx.check(!groups.empty() && groups.front().id == bs::service::groups::kFavorites, "favorites sort first");
    bs::core::WarpTarget t;
    t.id     = "x";
    t.kind   = "waystone";
    t.global = true;
    t.pos    = {1, 0, 0, 0};
    auto of  = bs::service::get<bs::service::IGroups>().groupsOf(p, t);
    auto has = [&](std::string const& id) { return std::find(of.begin(), of.end(), id) != of.end(); };
    ctx.check(has(std::string(bs::service::groups::kGlobal)), "global waystones are in the global group");
    ctx.check(has(bs::service::groups::dimension(1)), "dimension group");
    ctx.check(!bs::service::openScreen(p, "no_such_screen"), "unknown screens do not open");
});

#include "service/WarpEvents.h"

namespace {
int gWarpedEvents = 0;
}
BS_REGISTER_WARP_LISTENER([](Player&, bs::service::WarpRequest const& r, bs::service::Destination const&) {
    if (r.source.itemId == "bedstones:foundation_probe") ++gWarpedEvents;
});

BS_REGISTER_SELFTEST("foundation.warp_events", [](Context& ctx) {
    DefaultTeleportScope useDefault;
    auto& p  = ctx.player();
    auto& r  = ctx.createWaystone(sampleRecord(ctx, 52, "Evented"));
    int before = gWarpedEvents;
    bs::service::WarpRequest req;
    req.source.kind   = bs::core::WarpSourceKind::WarpScroll;
    req.source.itemId = "bedstones:foundation_probe";
    req.targetId      = r.uid;
    ctx.check(bs::service::get<bs::service::ITeleport>().warp(p, req).ok, "warp ok");
    ctx.check(gWarpedEvents == before + 1, "warp listeners run after a successful warp");
});

namespace {
int gCloseSeen = 0;
}
BS_REGISTER_SCREEN_ACTIONS("foundation_close_probe", [](Player&, bs::net::screens::ScreenSession&, std::string const& action, nlohmann::json const&) {
    if (action == "close") ++gCloseSeen;
});

BS_REGISTER_SELFTEST("foundation.screen_close", [](Context& ctx) {
    auto& p = ctx.player();
    bs::net::setTestSink([](Player&, std::string_view, nlohmann::json const&) {});
    bs::net::setCompanion(p, bs::net::CompanionInfo{"test", {"native_screen"}});
    int before = gCloseSeen;
    bs::net::screens::open(p, "foundation_close_probe", {});
    bs::net::screens::simulateAction(p, "close");
    ctx.check(gCloseSeen == before + 1, "the close action reaches the screen's handlers");
    ctx.check(bs::net::screens::current(p) == nullptr, "the session is gone after close");
    bs::net::setTestSink(nullptr);
    bs::net::setCompanion(p, std::nullopt);
});
