// Selftests of the admin commands (`/waystones selftest "commands.*"`). Test area dx 1600..1699.

#include "mc/server/SimulatedPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"

#include "feature/commands/AdminCommands.h"
#include "game/Blocks.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "selftest/SelfTest.h"

namespace {

using bs::selftest::Context;
namespace cmd = bs::feature::commands;

bs::core::WaystoneRecord record(Context& ctx, int dx, std::string name, std::string owner = {}) {
    bs::core::WaystoneRecord r;
    r.name     = std::move(name);
    r.pos      = ctx.testPos(dx);
    r.style    = "waystone";
    r.origin   = bs::core::Origin::Player;
    r.ownerUid = std::move(owner);
    return r;
}

/// Test players may keep data from earlier runs (same name -> same uuid): start clean.
void fresh(Player& p) {
    auto& ws = bs::game::Waystones::get();
    auto  key = bs::game::Waystones::playerKey(p);
    auto& d   = ws.playerData(p);
    d.activated.clear();
    d.sortingIndex.clear();
    d.cooldowns.clear();
    d.aliases.clear();
    ws.markPlayerDirty(key);
}

void expectText(Context& ctx, cmd::Reply const& r, std::string const& expected, std::string const& what) {
    ctx.check(r.text() == expected, what + ": expected '" + expected + "', got '" + r.text() + "'");
}

} // namespace

BS_REGISTER_SELFTEST("commands.activate_forget", [](Context& ctx) {
    auto& ws   = bs::game::Waystones::get();
    auto& p    = ctx.player();
    fresh(p);
    auto  name = p.getRealName();
    auto& rec  = ctx.createWaystone(record(ctx, 1600, "Cmd Alpha"));
    auto  uid  = rec.uid;
    auto  pos  = rec.pos;
    std::vector<Player*> targets{&p};

    auto r = cmd::activate(nullptr, targets, pos);
    ctx.check(r.ok && ws.isActivated(p, uid), "activate activates the waystone");
    expectText(ctx, r, "Activated waystone Cmd Alpha for " + name, "activate message");

    r = cmd::forget(nullptr, targets, pos);
    ctx.check(r.ok && !ws.isActivated(p, uid), "forget deactivates the waystone");
    expectText(ctx, r, "Deactivated waystone Cmd Alpha for " + name, "forget message");

    auto& other = ctx.createWaystone(record(ctx, 1602, "Cmd Beta"));
    ws.activate(p, uid);
    ws.activate(p, other.uid);
    r = cmd::forgetAll(nullptr, targets);
    ctx.check(ws.playerData(p).activated.empty(), "forget all clears the activated list");
    expectText(ctx, r, "Deactivated all waystones for " + name, "forget all message");

    r = cmd::activate(nullptr, targets, ctx.testPos(1604));
    ctx.check(!r.ok, "no waystone at an empty position");
    expectText(ctx, r, "Waystone not found at this position", "not found message");
    r = cmd::forget(nullptr, targets, ctx.testPos(1604));
    ctx.check(!r.ok && r.text() == "Waystone not found at this position", "forget: not found");
});

BS_REGISTER_SELFTEST("commands.multiple_players", [](Context& ctx) {
    auto& ws  = bs::game::Waystones::get();
    auto& p   = ctx.player();
    Player& q = ctx.spawnPlayer("BsCmdSecond");
    fresh(p);
    fresh(q);
    auto& rec = ctx.createWaystone(record(ctx, 1606, "Cmd Gamma"));
    auto  uid = rec.uid;
    std::vector<Player*> targets{&p, &q};
    auto r = cmd::activate(nullptr, targets, rec.pos);
    ctx.check(ws.isActivated(p, uid) && ws.isActivated(q, uid), "both players activated");
    expectText(ctx, r, "Activated waystone Cmd Gamma for 2 players", "multiple activate message");
    r = cmd::forget(nullptr, targets, rec.pos);
    expectText(ctx, r, "Deactivated waystone Cmd Gamma for 2 players", "multiple forget message");
    r = cmd::forgetAll(nullptr, targets);
    expectText(ctx, r, "Deactivated all waystones for 2 players", "multiple forget all message");
    r = cmd::cooldownResetAll(nullptr, targets);
    expectText(ctx, r, "Reset all cooldowns for 2 players", "multiple cooldown message");
});

BS_REGISTER_SELFTEST("commands.upper_half", [](Context& ctx) {
    // With the waystone block registered (U1), the upper half resolves to the record below.
    auto  id = bs::ids::id("waystone");
    if (!bs::game::blockWith(id)) {
        ctx.info("bedstones:waystone not registered: upper-half lookup skipped");
        return;
    }
    auto& rec = ctx.createWaystone(record(ctx, 1608, "Cmd Upper"));
    auto  pos = rec.pos;
    auto  up  = pos;
    up.y += 1;
    ctx.setBlock(pos, id, {{std::string(bs::ids::kStateUpper), 0}});
    ctx.setBlock(up, id, {{std::string(bs::ids::kStateUpper), 1}});
    ctx.check(cmd::waystoneAt(up) == bs::game::Waystones::get().findAt(pos), "upper half -> lower record");
});

BS_REGISTER_SELFTEST("commands.place", [](Context& ctx) {
    auto& ws  = bs::game::Waystones::get();
    auto  pos = ctx.testPos(1610);
    auto  up  = pos;
    up.y += 1;
    ctx.cleanupBlock(pos);
    ctx.cleanupBlock(up);
    auto r = cmd::place(nullptr, pos, "mossy_waystone", "Placed By Command");
    auto* rec = ws.findAt(pos);
    ctx.require(r.ok && rec != nullptr, "place creates a record: " + r.text());
    ctx.cleanupWaystone(rec->uid);
    ctx.check(rec->name == "Placed By Command", "named");
    ctx.check(rec->style == "mossy_waystone" && rec->isWaystone(), "style + type");
    ctx.check(rec->origin == bs::core::Origin::Player && !rec->hasOwner(), "player origin without owner");
    bool hasBlock = bs::game::blockWith(bs::ids::id("mossy_waystone")) != nullptr;
    if (hasBlock) {
        ctx.check(r.lines.empty(), "no output on success");
    } else {
        ctx.check(r.text().find("not registered") != std::string::npos, "missing block reported");
    }

    auto bad = cmd::place(nullptr, ctx.testPos(1612), "granite_waystone", "Nope");
    ctx.check(!bad.ok, "unknown style fails");
    expectText(ctx, bad, "Unknown waystone style: granite_waystone", "unknown style message");
    ctx.check(ws.findAt(ctx.testPos(1612)) == nullptr, "no record for an unknown style");

    auto ns = cmd::place(nullptr, ctx.testPos(1614), "waystones:sandy_waystone", "Namespaced");
    if (auto* n = ws.findAt(ctx.testPos(1614))) {
        ctx.cleanupWaystone(n->uid);
        ctx.check(n->style == "sandy_waystone", "namespaced style accepted");
    } else {
        ctx.fail("namespaced style: " + ns.text());
    }
    auto p2 = ctx.testPos(1614);
    ctx.cleanupBlock(p2);
    p2.y += 1;
    ctx.cleanupBlock(p2);
});

BS_REGISTER_SELFTEST("commands.count_list", [](Context& ctx) {
    auto& ws   = bs::game::Waystones::get();
    auto& p    = ctx.player();
    fresh(p);
    auto  key  = bs::game::Waystones::playerKey(p);
    auto  name = p.getRealName();
    auto& a    = ctx.createWaystone(record(ctx, 1620, "Owned Near", key));
    auto& b    = ctx.createWaystone(record(ctx, 1640, "Owned Far", key));
    auto& c    = ctx.createWaystone(record(ctx, 1630, "Visited"));
    auto  nr   = record(ctx, 1622, "Nether One");
    nr.pos.dim = 1;
    auto& d    = ctx.createWaystone(nr);
    auto& e    = ctx.createWaystone(record(ctx, 1624, "Not Activated", key)); // owned, not activated: not counted
    (void)e;
    for (auto* r : {&c, &d, &b, &a}) ws.activate(p, r->uid);

    expectText(ctx, cmd::count(nullptr, p), name + " has 4 waystones activated and owns 2 of those", "count");

    // Viewpoint at the first waystone's centre: distances 0, 10, 20 blocks.
    bs::core::commands::Viewpoint at{a.pos.dim, a.pos.x + 0.5, a.pos.y + 0.5, a.pos.z + 0.5};
    auto owned = cmd::list(nullptr, at, p, false);
    std::vector<std::string> want{
        "Waystones (x, y, z) by " + name + " (owned only):",
        "-     owned at 0 blocks away (§e" + std::to_string(a.pos.x) + ", " + std::to_string(a.pos.y) + ", "
            + std::to_string(a.pos.z) + "§r): §aOwned Near§r",
        "-     owned at 20 blocks away (§e" + std::to_string(b.pos.x) + ", " + std::to_string(b.pos.y) + ", "
            + std::to_string(b.pos.z) + "§r): §aOwned Far§r",
        "2 waystones owned",
    };
    ctx.check(owned.lines == want, "list (owned only):\n" + owned.text());

    auto all = cmd::list(nullptr, at, p, true);
    ctx.require(all.lines.size() == 6, "list all has header + 4 entries + footer:\n" + all.text());
    ctx.check(all.lines[0] == "Waystones (x, y, z) by " + name + ":", "all header");
    ctx.check(all.lines[1].find("Owned Near") != std::string::npos, "owned first");
    ctx.check(all.lines[2].find("Owned Far") != std::string::npos, "owned sorted by distance");
    ctx.check(all.lines[3].starts_with("- activated at 10 blocks away") && all.lines[3].find("Visited") != std::string::npos, "activated entry");
    ctx.check(all.lines[4].starts_with("- activated in minecraft:the_nether"), "other dimension last, 'in <dim>'");
    ctx.check(all.lines[5] == "4 waystones total, 2 owned", "all footer");

    // Aliases are shown (personalized names).
    ws.playerData(p).aliases[c.uid] = "My Alias";
    auto aliased = cmd::list(nullptr, at, p, true);
    ctx.check(aliased.text().find("§aMy Alias§r") != std::string::npos, "list shows the personal alias");
    ws.playerData(p).aliases.erase(c.uid);
});

BS_REGISTER_SELFTEST("commands.cooldown", [](Context& ctx) {
    auto& ws   = bs::game::Waystones::get();
    auto& p    = ctx.player();
    auto  name = p.getRealName();
    fresh(p);
    std::vector<Player*> t{&p};
    auto& cd = ws.playerData(p).cooldowns;

    auto before = cmd::nowMs();
    auto r      = cmd::cooldownSet(nullptr, t, "inventory_button", 60);
    auto after  = cmd::nowMs();
    expectText(ctx, r, "Set cooldown waystones:inventory_button to 60 seconds for " + name, "set message");
    ctx.require(cd.contains("waystones:inventory_button"), "set stores the namespaced id");
    auto v = cd["waystones:inventory_button"];
    ctx.check(v >= before + 60000 && v <= after + 60000, "set = now + 60 s");

    r = cmd::cooldownAdd(nullptr, t, "waystones:inventory_button", 30);
    expectText(ctx, r, "Added 30 seconds to cooldown waystones:inventory_button for " + name, "add message");
    ctx.check(cd["waystones:inventory_button"] == v + 30000, "add extends a running cooldown");

    r = cmd::cooldownAdd(nullptr, t, "other", 10);
    ctx.check(cd.contains("waystones:other") && cd["waystones:other"] >= before + 10000, "add on a missing cooldown starts from now");

    r = cmd::cooldownReset(nullptr, t, "inventory_button");
    expectText(ctx, r, "Reset cooldown waystones:inventory_button for " + name, "reset message");
    ctx.check(!cd.contains("waystones:inventory_button") && cd.contains("waystones:other"), "reset removes one cooldown");

    r = cmd::cooldownResetAll(nullptr, t);
    expectText(ctx, r, "Reset all cooldowns for " + name, "reset all message");
    ctx.check(cd.empty(), "reset all clears the cooldowns");

    r = cmd::cooldownSet(nullptr, t, "inventory_button", -1);
    ctx.check(!r.ok && cd.empty(), "set rejects negative seconds");
});

BS_REGISTER_SELFTEST("commands.twinbound", [](Context& ctx) {
    auto& p   = ctx.player();
    Player& q = ctx.spawnPlayer("BsCmdFeather");
    auto  r   = cmd::twinbound(nullptr, p, p);
    expectText(ctx, r, "Choose two different players", "same player");

    if (bs::game::makeItem(bs::ids::id(bs::ids::kTwinboundFeather)).isNull()) {
        r = cmd::twinbound(nullptr, p, q);
        ctx.check(!r.ok && r.text().find("not registered") != std::string::npos, "missing feather item reported");
    }
    // The linking logic, with a vanilla stand-in for the feather item.
    bs::feature::commands::detail::setFeatherItemForTest("minecraft:feather");
    struct Restore {
        ~Restore() { bs::feature::commands::detail::setFeatherItemForTest({}); }
    } restore;

    r = cmd::twinbound(nullptr, p, q);
    expectText(ctx, r, "Linked Twinbound Feathers for " + p.getRealName() + " and " + q.getRealName(), "success");
    auto isFeather = [](ItemStack const& s) { return bs::game::idOf(s) == "minecraft:feather"; };
    auto pl = bs::game::findItem(p, isFeather);
    auto ql = bs::game::findItem(q, isFeather);
    ctx.require(pl && ql, "both players got a feather");
    auto pid   = bs::game::userString(bs::game::itemAt(p, *pl), bs::ids::kNbtFeather);
    auto plink = bs::game::userString(bs::game::itemAt(p, *pl), bs::ids::kNbtFeatherLink);
    auto qid   = bs::game::userString(bs::game::itemAt(q, *ql), bs::ids::kNbtFeather);
    auto qlink = bs::game::userString(bs::game::itemAt(q, *ql), bs::ids::kNbtFeatherLink);
    ctx.check(pid && qid && *pid != *qid, "feathers have distinct ids");
    ctx.check(plink == qid && qlink == pid, "feathers link to each other");

    r = cmd::twinbound(nullptr, p, q);
    ctx.check(!r.ok, "second link reports already linked");
    expectText(ctx, r, p.getRealName() + " and " + q.getRealName() + " already have linked Twinbound Feathers", "already linked");

    // A full inventory without a feather: no_space.
    Player& full = ctx.spawnPlayer("BsCmdFull");
    int     size = full.getInventory().getContainerSize();
    for (int i = 0; i < size; ++i) ctx.give(full, bs::game::makeItem("minecraft:stone", 1), i);
    r = cmd::twinbound(nullptr, p, full);
    expectText(ctx, r, full.getRealName() + " needs a free inventory slot for a Twinbound Feather", "no space");
    // Re-linking p with another player keeps p's feather id and points it at the new partner.
    Player& third = ctx.spawnPlayer("BsCmdThird");
    r = cmd::twinbound(nullptr, p, third);
    ctx.check(r.ok, "re-link with another player");
    auto pl2 = bs::game::findItem(p, isFeather);
    ctx.check(pl2 && bs::game::userString(bs::game::itemAt(p, *pl2), bs::ids::kNbtFeather) == pid, "existing feather id kept");

    // A stack of feathers is never linked as a whole: a single new feather is handed out.
    Player& stacked = ctx.spawnPlayer("BsCmdStack");
    ctx.give(stacked, bs::game::makeItem("minecraft:feather", 2), 0);
    r = cmd::twinbound(nullptr, q, stacked);
    ctx.check(r.ok, "link with a player holding a feather stack");
    auto const& stack = bs::game::itemAt(stacked, {bs::game::ItemLocation::Kind::Inventory, 0});
    ctx.check(stack.mCount == 2 && !bs::game::userString(stack, bs::ids::kNbtFeather), "the stack stays untouched");
});

BS_REGISTER_SELFTEST("commands.gui", [](Context& ctx) {
    auto& p = ctx.player();
    auto  r = cmd::gui(p, p);
    ctx.check(r.ok && r.lines.empty(), "gui opens silently");
});
