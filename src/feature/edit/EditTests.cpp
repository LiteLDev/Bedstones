// Selftests of the edit screen (`edit.*`): save validation and visibility options.
// Test area: ctx.testPos(1300..1349).

#include <cmath>

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/GameType.h"
#include "mc/world/level/dimension/DimensionType.h"

#include "feature/edit/Edit.h"
#include "feature/modifiers/Modifiers.h"
#include "game/BlockContainer.h"
#include "game/Waystones.h"
#include "net/Companion.h"
#include "net/Screens.h"
#include "selftest/SelfTest.h"
#include "service/Services.h"

namespace {

using bs::selftest::Context;
namespace screens = bs::net::screens;

/// Marks the test player as a client-mod player whose packets go nowhere (restored on exit).
struct CompanionScope {
    Player& p;
    explicit CompanionScope(Player& player) : p(player) {
        bs::net::setTestSink([](Player&, std::string_view, nlohmann::json const&) {});
        bs::net::setCompanion(p, bs::net::CompanionInfo{"test", {"native_screen"}});
    }
    ~CompanionScope() {
        screens::close(p);
        bs::net::setCompanion(p, std::nullopt);
        bs::net::setTestSink(nullptr);
    }
};

/// Moves the test player to the unit's test area and returns the block next to its feet.
ll::coro::CoroTask<bs::core::GlobalPos> placePlayer(Context& ctx, int dx) {
    auto& p   = ctx.player();
    auto  at  = ctx.testPos(dx);
    p.teleport(Vec3{at.x + 0.5f, at.y + 1.f, at.z + 0.5f}, DimensionType(0), Vec2{0.f, 0.f});
    co_await ctx.ticks(3);
    auto feet = p.getFeetPos();
    co_return bs::core::GlobalPos{0, static_cast<int>(std::floor(feet.x)) + 1, static_cast<int>(std::floor(feet.y)), static_cast<int>(std::floor(feet.z))};
}

bs::core::WaystoneRecord record(Player& owner, bs::core::GlobalPos pos, std::string name = "Before") {
    bs::core::WaystoneRecord r;
    r.name      = std::move(name);
    r.pos       = pos;
    r.style     = "waystone";
    r.origin    = bs::core::Origin::Player;
    r.ownerUid  = bs::game::Waystones::playerKey(owner);
    r.ownerName = owner.getRealName();
    return r;
}

} // namespace

BS_REGISTER_SELFTEST("edit.save_rules", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = ctx.player();
    auto  pos = co_await placePlayer(ctx, 1300);
    auto  uid = ctx.createWaystone(record(p, pos)).uid;
    CompanionScope scope(p);
    ctx.require(bs::feature::edit::openEdit(p, uid), "edit screen opens");
    auto* s = screens::current(p, "edit");
    ctx.require(s != nullptr, "edit screen is current");
    ctx.check(s->model.value("name", "") == "Before", "model carries the name");
    ctx.check(s->model.value("canEdit", false), "the owner may edit");
    screens::simulateAction(p, "save", {{"name", ""}, {"visibility", "bogus"}});
    auto* r = bs::game::Waystones::get().find(uid);
    ctx.require(r != nullptr, "record still there");
    ctx.check(r->name == "Untitled Waystone", "empty name becomes Untitled Waystone (got '" + r->name + "')");
    auto opts = bs::service::get<bs::service::IPermissions>().visibilityOptions(p, *r);
    ctx.check(!opts.empty() && r->visibility == opts.front(), "invalid visibility falls back to the first option");
    ctx.check(screens::current(p) == nullptr, "save closes the screen");
    bs::feature::edit::openEdit(p, uid);
    screens::simulateAction(p, "save", {{"name", std::string(140, 'x')}, {"visibility", "activation"}});
    r = bs::game::Waystones::get().find(uid);
    ctx.check(r && r->name.size() == 128, "name clipped to 128 characters");
    ctx.check(bs::feature::edit::save(p, uid, "Renamed", "activation") == bs::feature::edit::SaveResult::Saved, "direct save");
    r = bs::game::Waystones::get().find(uid);
    ctx.check(r && r->name == "Renamed", "renamed");
});

BS_REGISTER_SELFTEST("edit.permission_denied", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = ctx.player();
    auto  pos = co_await placePlayer(ctx, 1310);
    auto  rec = record(p, pos, "Theirs");
    rec.ownerUid  = "00000000-0000-4000-8000-000000000001";
    rec.ownerName = "Someone";
    auto uid      = ctx.createWaystone(rec).uid;
    auto mode     = p.getPlayerGameType();
    p.setPlayerGameType(GameType::Survival);
    {
        CompanionScope scope(p);
        bs::feature::edit::openEdit(p, uid);
        auto* s = screens::current(p, "edit");
        ctx.check(s && !s->model.value("canEdit", true), "read-only for a non-owner");
        ctx.check(s && s->model["error"].is_object(), "error line present");
        screens::simulateAction(p, "save", {{"name", "Hijacked"}, {"visibility", "activation"}});
        auto* r = bs::game::Waystones::get().find(uid);
        ctx.check(r && r->name == "Theirs", "a denied save changes nothing");
        ctx.check(bs::feature::edit::save(p, uid, "X", "") == bs::feature::edit::SaveResult::Denied, "save reports Denied");
    }
    p.setPlayerGameType(mode);
});

BS_REGISTER_SELFTEST("edit.distance", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = ctx.player();
    auto  pos = co_await placePlayer(ctx, 1320);
    auto  far = pos;
    far.x += 6;
    auto uid = ctx.createWaystone(record(p, far, "Far")).uid;
    ctx.check(bs::feature::edit::save(p, uid, "Changed", "activation") == bs::feature::edit::SaveResult::TooFar, "more than 4 blocks away");
    auto* r = bs::game::Waystones::get().find(uid);
    ctx.check(r && r->name == "Far", "unchanged");
    auto nearUid = ctx.createWaystone(record(p, pos, "Near")).uid;
    ctx.check(bs::feature::edit::save(p, nearUid, "Changed", "activation") == bs::feature::edit::SaveResult::Saved, "within 4 blocks");
});

BS_REGISTER_SELFTEST("edit.visibility_options", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p    = ctx.player();
    auto  pos  = co_await placePlayer(ctx, 1330);
    auto  uid  = ctx.createWaystone(record(p, pos, "Opts")).uid;
    auto& perm = bs::service::get<bs::service::IPermissions>();
    auto  mode = p.getPlayerGameType();
    p.setPlayerGameType(GameType::Creative);
    {
        CompanionScope scope(p);
        bs::feature::edit::openEdit(p, uid);
        auto* s = screens::current(p, "edit");
        ctx.require(s != nullptr, "screen open");
        auto* r    = bs::game::Waystones::get().find(uid);
        auto  opts = perm.visibilityOptions(p, *r);
        auto  sent = s->model.value("options", std::vector<std::string>{});
        ctx.check(sent.size() == opts.size(), "model lists every option");
        for (std::size_t i = 0; i < sent.size() && i < opts.size(); ++i) {
            ctx.check(sent[i] == bs::core::toString(opts[i]), "option order " + std::to_string(i));
        }
        // Cycling: each option saved in turn sticks.
        for (auto v : opts) {
            bs::feature::edit::save(p, uid, "Opts", std::string(bs::core::toString(v)));
            r = bs::game::Waystones::get().find(uid);
            ctx.check(r && r->visibility == v, "visibility " + std::string(bs::core::toString(v)) + " saved");
        }
        // A sharestone has only its own visibility.
        auto share  = record(p, {pos.dim, pos.x, pos.y, pos.z + 1}, "Share");
        share.type  = bs::core::types::sharestone("red");
        share.visibility = bs::core::Visibility::Sharestone;
        auto suid   = ctx.createWaystone(share).uid;
        auto model  = bs::feature::edit::buildModel(p, suid);
        auto sopts  = model.value("options", std::vector<std::string>{});
        ctx.check(sopts.size() == 1 && sopts[0] == "sharestone", "sharestones only offer their own visibility");
        ctx.check(model.value("kind", "") == "sharestone" && model.value("sharestoneColor", "") == "red", "sharestone kind and colour");
        ctx.check(!model.value("modifiersAvailable", true), "sharestones have no modifier container");
    }
    p.setPlayerGameType(mode);
});

BS_REGISTER_SELFTEST("edit.modifier_count", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = ctx.player();
    auto  pos = co_await placePlayer(ctx, 1340);
    auto& rec = ctx.createWaystone(record(p, pos, "Counted"));
    auto  c   = bs::game::containerOf(rec);
    c[1]      = bs::game::SlotItem{"minecraft:blaze_powder", 2, 0, {}};
    c[2]      = bs::game::SlotItem{"minecraft:amethyst_shard", 5, 0, {}};
    c[3]      = bs::game::SlotItem{"minecraft:diamond", 1, 0, {}};
    bs::game::setContainer(rec, c);
    auto uid = rec.uid;
    ctx.check(bs::feature::modifiers::activeModifierCount(rec) == 3, "amethyst shards are not counted");
    auto model = bs::feature::edit::buildModel(p, uid);
    ctx.check(model.value("modifierCount", 0) == 3, "edit model shows the count");
});
