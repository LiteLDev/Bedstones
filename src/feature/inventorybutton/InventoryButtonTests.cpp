// Selftests of the inventory button (test area dx 1400..1499).

#include <cmath>

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/GameType.h"
#include "mc/world/level/dimension/DimensionType.h"

#include "feature/inventorybutton/CooldownRequirements.h"
#include "feature/inventorybutton/InventoryButton.h"
#include "game/Waystones.h"
#include "mod/Config.h"
#include "net/Companion.h"
#include "net/Messages.h"
#include "selftest/SelfTest.h"
#include "service/Services.h"

namespace bs::feature::inventorybutton {
namespace {

using core::inventorybutton::Mode;
using selftest::Context;

/// Restores the config section after the test.
struct ConfigGuard {
    InventoryButtonConfig saved = config::get<InventoryButtonConfig>();
    ~ConfigGuard() { config::mut<InventoryButtonConfig>() = saved; }
    static void mode(std::string v) { config::mut<InventoryButtonConfig>().inventoryButton = std::move(v); }
};

/// Cleans the player data keys the tests touch.
struct PlayerDataGuard {
    Player& player;
    ~PlayerDataGuard() {
        auto& data = game::Waystones::get().playerData(player);
        data.extra.erase("returnPortal");
        data.cooldowns.erase(kCooldownId);
    }
};

/// Selection recorder (ANY mode).
struct RecordingSelection final : service::ISelection {
    int*                  opened;
    core::WarpSourceKind* kind;
    RecordingSelection(int* o, core::WarpSourceKind* k) : opened(o), kind(k) {}
    void open(Player&, core::WarpSource const& source) override {
        ++*opened;
        *kind = source.kind;
    }
    void openAdmin(Player&, std::string const&) override {}
};

core::WaystoneRecord record(Context& ctx, int dx, std::string name, int dim = 0) {
    core::WaystoneRecord r;
    r.name    = std::move(name);
    r.pos     = ctx.testPos(dx);
    r.pos.dim = dim;
    r.style   = "waystone";
    r.origin  = core::Origin::Player;
    r.facing  = 2;
    return r;
}

void moveTo(Player& p, core::GlobalPos const& pos) {
    p.teleport(
        Vec3{static_cast<float>(pos.x) + 0.5f, static_cast<float>(pos.y) + 1.f, static_cast<float>(pos.z) + 0.5f},
        DimensionType(pos.dim),
        Vec2{0.f, 0.f}
    );
}

bool near(Player& p, core::GlobalPos const& pos, float range = 2.5f) {
    auto v = p.getPosition();
    return std::abs(v.x - (static_cast<float>(pos.x) + 0.5f)) < range
        && std::abs(v.z - (static_cast<float>(pos.z) + 0.5f)) < range;
}

} // namespace

BS_REGISTER_SELFTEST("inventorybutton.state_modes", [](Context& ctx) {
    ConfigGuard cfg;
    auto&       ws  = game::Waystones::get();
    auto&       p   = ctx.player();
    auto&       rec = ctx.createWaystone(record(ctx, 1400, "IB Home"));
    auto        uid = rec.uid;

    ConfigGuard::mode("");
    auto s = computeState(p);
    ctx.check(s.mode == Mode::None, "empty config disables the button");
    ctx.check(
        s.x == cfg.saved.inventoryButtonX && s.creativeY == cfg.saved.creativeInventoryButtonY,
        "positions come from the server config"
    );
    ConfigGuard::mode("NONE");
    ctx.check(computeState(p).mode == Mode::None, "NONE disables the button");

    ConfigGuard::mode("NEAREST");
    s = computeState(p);
    ctx.check(s.mode == Mode::Nearest && !s.hasTarget, "nearest without activated waystones has no target");
    ConfigGuard::mode("ANY");
    ctx.check(!computeState(p).hasTarget, "any: nothing activated yet");

    ws.activate(p, uid);
    ConfigGuard::mode("NEAREST");
    s = computeState(p);
    ctx.check(s.hasTarget && s.targetId == uid && s.targetName == "IB Home", "nearest targets the activated waystone");
    ctx.check(s.canAfford, "default requirements are affordable");

    ConfigGuard::mode("IB Home");
    s = computeState(p);
    ctx.check(s.mode == Mode::Named && s.hasTarget && s.targetId == uid, "named mode matches by name");
    ConfigGuard::mode("Elsewhere");
    s = computeState(p);
    ctx.check(s.mode == Mode::Named && !s.hasTarget, "unknown name has no target");

    ws.playerData(p).aliases[uid] = "My Base";
    ConfigGuard::mode("IB Home");
    s = computeState(p);
    ctx.check(s.hasTarget && s.targetName == "My Base", "the target is shown with the player's alias");
    ws.playerData(p).aliases.erase(uid);

    ConfigGuard::mode("ANY");
    s = computeState(p);
    ctx.check(
        s.mode == Mode::Any && s.hasTarget && s.targetId.empty(),
        "any: has activated waystones, no fixed target"
    );
    ws.forget(p, uid);
});

BS_REGISTER_SELFTEST("inventorybutton.nearest", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard     cfg;
    auto&           ws = game::Waystones::get();
    auto&           p  = ctx.player();
    PlayerDataGuard pd{p};
    ConfigGuard::mode("NEAREST");
    auto a = ctx.createWaystone(record(ctx, 1410, "IB A")).uid;
    auto b = ctx.createWaystone(record(ctx, 1440, "IB B")).uid;
    auto c = ctx.createWaystone(record(ctx, 1400, "IB Nether", 1)).uid; // same x/z, other dimension
    auto d = ctx.createWaystone(record(ctx, 1404, "IB Unknown")).uid;   // nearest, but not activated
    for (auto const& uid : {a, b, c}) ws.activate(p, uid);
    (void)d;

    moveTo(p, ctx.testPos(1400));
    co_await ctx.ticks(2);
    auto s = computeState(p);
    ctx.check(s.targetId == a, "nearest activated waystone in the player's dimension (A), got " + s.targetName);

    moveTo(p, ctx.testPos(1445));
    co_await ctx.ticks(2);
    s = computeState(p);
    ctx.check(s.targetId == b, "moving changes the nearest target (B), got " + s.targetName);

    // A return portal wins over every waystone, even a nearer one.
    auto portal                            = record(ctx, 1480, "IB Portal");
    portal.type                            = std::string(core::types::kWarpPortal);
    portal.style                           = "warp_portal";
    auto portalUid                         = ctx.createWaystone(portal).uid;
    ws.playerData(p).extra["returnPortal"] = portalUid;
    s                                      = computeState(p);
    ctx.check(s.targetId == portalUid, "the return portal is preferred");

    ws.playerData(p).extra["returnPortal"] = "missing-uid";
    s                                      = computeState(p);
    ctx.check(s.targetId == b, "a vanished return portal falls back to the nearest waystone");
    ws.playerData(p).extra["returnPortal"] = a; // not a portal record: ignored
    ctx.check(computeState(p).targetId == b, "returnPortal must name a warp portal");
});

BS_REGISTER_SELFTEST("inventorybutton.warp_message", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard cfg;
    auto&       ws            = game::Waystones::get();
    auto&       p             = ctx.player();
    int         stateMessages = 0;
    net::setTestSink([&stateMessages](Player&, std::string_view, nlohmann::json const& body) {
        if (body.value("channel", std::string{}) == kStateChannel) ++stateMessages;
    });
    net::setCompanion(p, net::CompanionInfo{"test", {"native_screen"}});

    auto rec = ctx.createWaystone(record(ctx, 1470, "IB Target"));
    ws.activate(p, rec.uid);

    // Disabled: the message is ignored.
    ConfigGuard::mode("");
    moveTo(p, ctx.testPos(1450));
    co_await ctx.ticks(2);
    net::simulateClientMessage(
        p,
        kWarpChannel,
        {
            {"targetId", rec.uid}
    }
    );
    co_await ctx.ticks(2);
    ctx.check(!near(p, rec.pos), "disabled button does not warp");

    // NEAREST: a confirmed target that is not the server's choice is refused.
    ConfigGuard::mode("NEAREST");
    net::simulateClientMessage(
        p,
        kWarpChannel,
        {
            {"targetId", "forged"}
    }
    );
    co_await ctx.ticks(3);
    ctx.check(!near(p, rec.pos), "nearest: a stale / forged confirmed target does not warp");
    net::simulateClientMessage(
        p,
        kWarpChannel,
        {
            {"targetId", rec.uid}
    }
    );
    co_await ctx.ticks(3);
    ctx.check(near(p, rec.pos), "nearest: warped next to the waystone");
    ctx.check(stateMessages > 0, "the state is sent to the client");

    // Named.
    moveTo(p, ctx.testPos(1450));
    co_await ctx.ticks(2);
    ConfigGuard::mode(" IB Target ");
    net::simulateClientMessage(p, kWarpChannel, {});
    co_await ctx.ticks(3);
    ctx.check(near(p, rec.pos), "named: warped next to the waystone");

    // ANY: opens the selection menu with source InventoryButton.
    {
        int                           opened = 0;
        core::WarpSourceKind          kind   = core::WarpSourceKind::Command;
        SeamSwap<service::ISelection> swap(std::make_unique<RecordingSelection>(&opened, &kind));
        ConfigGuard::mode("ANY");
        net::simulateClientMessage(p, kWarpChannel, {});
        ctx.check(
            opened == 1 && kind == core::WarpSourceKind::InventoryButton,
            "any: selection opened from the inventory button"
        );
    }

    // Syncing only sends changes.
    stateMessages = 0;
    syncState(p, true);
    syncState(p);
    ctx.check(stateMessages == 1, "unchanged state is not sent again");

    net::setTestSink(nullptr);
    net::setCompanion(p, std::nullopt);
});

BS_REGISTER_SELFTEST("inventorybutton.cooldown", [](Context& ctx) -> ll::coro::CoroTask<> {
    ConfigGuard                      cfg;
    auto&                            ws = game::Waystones::get();
    auto&                            p  = ctx.player();
    PlayerDataGuard                  pd{p};
    SeamSwap<service::IRequirements> swap(std::make_unique<CooldownRequirements>());
    auto                             originalMode = p.getPlayerGameType();
    p.setPlayerGameType(GameType::Survival);
    ConfigGuard::mode("NEAREST");

    auto rec = ctx.createWaystone(record(ctx, 1490, "IB Cooldown"));
    ws.activate(p, rec.uid);
    ws.playerData(p).cooldowns.erase(kCooldownId);

    moveTo(p, ctx.testPos(1460));
    co_await ctx.ticks(2);
    auto s = computeState(p);
    ctx.check(s.canAfford && s.cooldownSeconds == 0, "no cooldown at first");
    ctx.check(use(p), "first use warps");
    co_await ctx.ticks(3);
    ctx.check(near(p, rec.pos), "warped");

    s = computeState(p);
    ctx.check(!s.canAfford, "on cooldown: unaffordable");
    ctx.check(
        s.cooldownSeconds >= 299 && s.cooldownSeconds <= 300,
        "cooldown ~300 s, got " + std::to_string(s.cooldownSeconds)
    );

    moveTo(p, ctx.testPos(1460));
    co_await ctx.ticks(2);
    net::simulateClientMessage(p, kWarpChannel, {});
    co_await ctx.ticks(3);
    ctx.check(!near(p, rec.pos), "the cooldown blocks the warp");

    // Creative: allowed, and using the button resets the cooldowns.
    p.setPlayerGameType(GameType::Creative);
    co_await ctx.ticks(1);
    ctx.check(use(p), "creative ignores the cooldown");
    co_await ctx.ticks(3);
    ctx.check(near(p, rec.pos), "creative warped");
    ctx.check(ws.playerData(p).cooldowns.count(kCooldownId) == 0, "creative use resets the cooldowns");
    p.setPlayerGameType(originalMode);
});

} // namespace bs::feature::inventorybutton
