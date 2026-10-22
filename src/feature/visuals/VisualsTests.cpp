// Selftests of the visuals sync (U17, test positions dx 1700..1799): payload contents for a
// test companion, activation flags, range filtering and re-sends on activate / forget / moving.

#include <algorithm>
#include <optional>

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/dimension/DimensionType.h"

#include "core/visuals/Visuals.h"
#include "feature/visuals/VisualsSync.h"
#include "game/Waystones.h"
#include "net/Companion.h"
#include "selftest/SelfTest.h"

namespace bs::feature::visuals {

namespace {

using selftest::Context;
using core::visuals::Entry;

core::WaystoneRecord record(Context& ctx, int dx, int dz, std::string type, std::string style) {
    core::WaystoneRecord r;
    r.name   = "Visuals " + style;
    r.pos    = ctx.testPos(dx, dz);
    r.type   = std::move(type);
    r.style  = std::move(style);
    r.origin = core::Origin::Player;
    if (core::types::isSharestone(r.type)) r.visibility = core::Visibility::Sharestone;
    return r;
}

void moveTo(Player& p, core::GlobalPos const& pos) {
    p.teleport(Vec3{pos.x + 0.5f, pos.y + 1.f, pos.z + 0.5f}, DimensionType(pos.dim), Vec2{0.f, 0.f});
}

std::optional<Entry> find(std::vector<Entry> const& entries, std::string const& uid) {
    auto it = std::find_if(entries.begin(), entries.end(), [&](Entry const& e) { return e.uid == uid; });
    if (it == entries.end()) return std::nullopt;
    return *it;
}

/// Marks the player as a companion and captures `visuals.waystones` payloads; undone on scope exit
/// (also when a test aborts).
class Capture {
public:
    explicit Capture(Player& p) : mPlayer(p) {
        net::setCompanion(p, net::CompanionInfo{"test", {"native_screen"}});
        net::setTestSink([this](Player&, std::string_view, nlohmann::json const& body) {
            if (body.value("channel", std::string{}) != kChannel) return;
            payloads.push_back(core::visuals::fromJson(body.value("data", nlohmann::json{})));
        });
    }
    ~Capture() {
        net::setTestSink(nullptr);
        net::setCompanion(mPlayer, std::nullopt);
    }
    Capture(Capture const&)            = delete;
    Capture& operator=(Capture const&) = delete;

    [[nodiscard]] std::vector<Entry> const* last() const { return payloads.empty() ? nullptr : &payloads.back(); }

    std::vector<std::vector<Entry>> payloads;

private:
    Player& mPlayer;
};

} // namespace

BS_REGISTER_SELFTEST("visuals.payload", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& ws = game::Waystones::get();
    auto& p  = ctx.player();
    moveTo(p, ctx.testPos(1750));
    co_await ctx.ticks(2);
    auto  plain  = ctx.createWaystone(record(ctx, 1700, 0, "waystone", "waystone")).uid;
    auto  black  = ctx.createWaystone(record(ctx, 1702, 0, "waystone", "blackstone_waystone")).uid;
    auto  share  = ctx.createWaystone(record(ctx, 1704, 0, "red_sharestone", "red_sharestone")).uid;
    auto  plate  = ctx.createWaystone(record(ctx, 1706, 0, "warp_plate", "warp_plate")).uid;
    auto  far    = ctx.createWaystone(record(ctx, 1708, 300, "waystone", "waystone")).uid;
    ws.activate(p, plain);

    auto entries = entriesFor(p);
    auto ePlain  = find(entries, plain);
    auto eBlack  = find(entries, black);
    auto eShare  = find(entries, share);
    ctx.require(ePlain && eBlack && eShare, "nearby waystones and sharestones are in the payload");
    ctx.check(ePlain->activated, "activated waystone is flagged");
    ctx.check(!eBlack->activated, "other waystone is not flagged");
    ctx.check(eBlack->style == "blackstone_waystone", "style is sent");
    ctx.check(core::visuals::runeColor(*eBlack) == 0x993333, "Black Waystone rune colour");
    ctx.check(eShare->type == "red_sharestone" && core::visuals::showsRunes(*eShare), "sharestones always show runes");
    ctx.check(ePlain->pos == ctx.testPos(1700), "position is sent");
    ctx.check(!find(entries, plate), "warp plates are not sent");
    ctx.check(!find(entries, far), "records farther than 128 blocks are not sent");
});

BS_REGISTER_SELFTEST("visuals.sync_updates", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& ws = game::Waystones::get();
    auto& p  = ctx.player();
    moveTo(p, ctx.testPos(1750));
    co_await ctx.ticks(2);
    Capture cap(p);
    auto    a = ctx.createWaystone(record(ctx, 1710, 0, "waystone", "waystone")).uid;
    auto    b = ctx.createWaystone(record(ctx, 1712, 0, "waystone", "end_stone_waystone")).uid;
    ctx.require(sync(p, true), "forced sync sends");
    ctx.require(cap.last() != nullptr, "payload reached the companion");
    ctx.check(find(*cap.last(), a) && !find(*cap.last(), a)->activated, "not activated at first");
    ctx.check(!sync(p), "an unchanged payload is not re-sent");

    auto before = cap.payloads.size();
    ws.activate(p, a);
    co_await ctx.ticks(2);
    ctx.require(cap.payloads.size() > before, "activation triggers a sync");
    ctx.check(find(*cap.last(), a) && find(*cap.last(), a)->activated, "activation flag set after activate");
    ctx.check(find(*cap.last(), b) && !find(*cap.last(), b)->activated, "other waystone unchanged");

    before = cap.payloads.size();
    ws.forget(p, a);
    co_await ctx.ticks(2);
    ctx.require(cap.payloads.size() > before, "forgetting triggers a sync");
    ctx.check(find(*cap.last(), a) && !find(*cap.last(), a)->activated, "activation flag cleared after forget");

    before = cap.payloads.size();
    auto c = ctx.createWaystone(record(ctx, 1714, 0, "lime_sharestone", "lime_sharestone")).uid;
    co_await ctx.ticks(2);
    ctx.require(cap.payloads.size() > before, "a new record triggers a sync");
    ctx.check(find(*cap.last(), c).has_value(), "new sharestone is sent");

    before = cap.payloads.size();
    ws.remove(c);
    co_await ctx.ticks(2);
    ctx.require(cap.payloads.size() > before, "a removed record triggers a sync");
    ctx.check(!find(*cap.last(), c), "removed sharestone is gone");
});

BS_REGISTER_SELFTEST("visuals.range_refresh", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p = ctx.player();
    moveTo(p, ctx.testPos(1750));
    co_await ctx.ticks(2);
    Capture cap(p);
    auto    a = ctx.createWaystone(record(ctx, 1720, 0, "waystone", "mossy_waystone")).uid;
    sync(p, true);
    ctx.require(cap.last() && find(*cap.last(), a), "nearby record sent");
    moveTo(p, ctx.testPos(1750, 400));
    co_await ctx.ticks(kRefreshTicks + 5);
    ctx.require(cap.last() != nullptr, "payload present");
    ctx.check(!find(*cap.last(), a), "moving away drops the record within one refresh");
    moveTo(p, ctx.testPos(1750));
    co_await ctx.ticks(kRefreshTicks + 5);
    ctx.check(cap.last() && find(*cap.last(), a), "moving back brings it back");
});

BS_REGISTER_SELFTEST("visuals.no_companion", [](Context& ctx) {
    auto& p = ctx.player();
    net::setCompanion(p, std::nullopt);
    ctx.check(!sync(p, true), "players without the client mod get nothing");
});

} // namespace bs::feature::visuals
