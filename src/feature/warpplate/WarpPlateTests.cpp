// Selftests of unit U3 (warp plate and shards). Test positions use dx 300..399.

#include <cmath>

#include "ll/api/service/Bedrock.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorCategory.h"
#include "mc/world/actor/item/ItemActor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/GameType.h"
#include "mc/world/level/dimension/DimensionType.h"
#include "mc/world/phys/AABB.h"

#include "core/warpplate/WarpPlateLogic.h"
#include "feature/warpplate/WarpPlate.h"
#include "game/BlockContainer.h"
#include "game/Blocks.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/Config.h"
#include "selftest/SelfTest.h"

namespace {

using bs::selftest::Context;
using namespace bs;
using feature::warpplate::WarpPlates;

core::GlobalPos above(core::GlobalPos p, int dy) { return {p.dim, p.x, p.y + dy, p.z}; }

int gSurfaceY = 0; ///< first free y above the ground of the current test area (set by `arrive`)

/// Test position `dx` at the surface found by `arrive` (the spawn y may be unset on BDS).
core::GlobalPos platePos(Context& ctx, int dx) {
    auto p = ctx.testPos(dx);
    p.y    = gSurfaceY;
    return p;
}

/// A plate block with a stone floor and free air above; returns its record uid.
std::string makePlate(Context& ctx, int dx, bool dormant = false) {
    auto pos = platePos(ctx, dx);
    ctx.setBlock(above(pos, -1), "minecraft:stone");
    ctx.setBlock(above(pos, 1), "minecraft:air");
    ctx.setBlock(above(pos, 2), "minecraft:air");
    ctx.require(ctx.setBlock(pos, feature::warpplate::plateBlockId()), "plate block exists (behavior pack loaded)");
    auto* r = WarpPlates::get().onPlaced(nullptr, pos, std::nullopt, dormant);
    ctx.require(r != nullptr, "plate record created");
    ctx.cleanupWaystone(r->uid);
    return r->uid;
}

void setShard(std::string const& plate, std::string const& id, std::string const& target, int slot = 0, int count = 1) {
    auto& ws = game::Waystones::get();
    auto* r  = ws.find(plate);
    if (!r) return;
    auto c = game::containerOf(*r);
    if (id.empty()) c[static_cast<std::size_t>(slot)].reset();
    else {
        game::SlotItem s{id, count, 0, {}};
        if (!target.empty()) s.user[std::string(ids::kNbtAttunement)] = target;
        c[static_cast<std::size_t>(slot)] = s;
    }
    game::setContainer(*r, c);
    ws.markDirty(plate);
}

std::optional<game::SlotItem> slot(std::string const& plate, int i) {
    auto* r = game::Waystones::get().find(plate);
    if (!r) return std::nullopt;
    return game::containerOf(*r)[static_cast<std::size_t>(i)];
}

void standOn(Player& p, core::GlobalPos const& pos) {
    p.teleport(
        Vec3{static_cast<float>(pos.x) + 0.5f, static_cast<float>(pos.y) + 0.15f, static_cast<float>(pos.z) + 0.5f},
        DimensionType(pos.dim)
    );
}

bool near(Player& p, core::GlobalPos const& pos, float r = 2.5f) {
    auto f = p.getFeetPos();
    return std::abs(f.x - (static_cast<float>(pos.x) + 0.5f)) <= r
        && std::abs(f.z - (static_cast<float>(pos.z) + 0.5f)) <= r && std::abs(f.y - static_cast<float>(pos.y)) <= 2.f;
}

int plateState(core::GlobalPos const& pos) {
    auto* region = &ll::service::getLevel()->getDimension(pos.dim).lock()->getBlockSourceFromMainChunkSource();
    return game::intState(region->getBlock(game::blockPos(pos)), ids::kStateStatus).value_or(-1);
}

int waitTicks() { return config::get<feature::warpplate::WarpPlateConfig>().warpPlateUseTime + 12; }

/// Moves the test player to the test area (dx..dx+span) and waits until its chunks are loaded.
ll::coro::CoroTask<> arrive(Context& ctx, int dx, int span = 20) {
    auto& p      = ctx.player();
    auto  pos    = ctx.testPos(dx + span / 2);
    int   hintY  = pos.y >= -60 && pos.y < 300 ? pos.y : 64;
    auto* region = &ll::service::getLevel()->getDimension(0).lock()->getBlockSourceFromMainChunkSource();
    for (int i = 0; i < 200; ++i) {
        standOn(p, {pos.dim, pos.x, hintY, pos.z + 4}); // held in place (no fall) until the chunks load
        auto a = ctx.testPos(dx), b = ctx.testPos(dx + span);
        if (region->hasChunksAt(BlockPos{a.x, hintY, a.z}, 2, false)
            && region->hasChunksAt(BlockPos{b.x, hintY, b.z}, 2, false))
            break;
        co_await ctx.ticks(2);
    }
    auto base = ctx.testPos(dx);
    gSurfaceY = region->getAboveTopSolidBlock(base.x, base.z, false, false);
    ctx.info("test area at y " + std::to_string(gSurfaceY));
    standOn(p, {pos.dim, pos.x, gSurfaceY, pos.z + 4});
    co_await ctx.ticks(2);
}

} // namespace

BS_REGISTER_SELFTEST("warpplate.galactic", [](Context& ctx) {
    auto uid  = std::string("3f2a1b4c-9d8e-4f7a-b6c5-d4e3f2a1b0c9");
    auto name = feature::warpplate::galacticName(uid);
    ctx.check(name.ends_with("fabcdefa"), "galactic identifier: " + name);
    ctx.check(name.starts_with("§"), "galactic name is coloured");
});

BS_REGISTER_SELFTEST("warpplate.attunement", [](Context& ctx) -> ll::coro::CoroTask<> {
    co_await arrive(ctx, 300);
    auto uid = makePlate(ctx, 300, true);
    auto pos = game::Waystones::get().find(uid)->pos;
    co_await ctx.ticks(3);
    ctx.check(plateState(pos) == static_cast<int>(ids::WarpPlateStatus::Attuning), "a fresh plate is attuning");
    ctx.check(slot(uid, 0) && slot(uid, 0)->id == ids::id(ids::kDormantShard), "a fresh plate holds a dormant shard");
    co_await ctx.ticks(core::warpplate::kAttunementTicks + 5);
    ctx.check(!slot(uid, 0), "the attuned shard was ejected");
    ctx.check(plateState(pos) == static_cast<int>(ids::WarpPlateStatus::Empty), "the plate is empty after ejection");
    auto& region = ll::service::getLevel()->getDimension(0).lock()->getBlockSourceFromMainChunkSource();
    AABB  box{
        Vec3{static_cast<float>(pos.x - 2), static_cast<float>(pos.y - 2), static_cast<float>(pos.z - 2)},
        Vec3{static_cast<float>(pos.x + 3), static_cast<float>(pos.y + 3), static_cast<float>(pos.z + 3)}
    };
    bool found = false;
    for (auto* a : region.getEntities(box, 0.f)) {
        if (!a || !a->hasCategory(ActorCategory::Item)) continue;
        auto& item = static_cast<ItemActor&>(*a).item();
        if (game::idOf(item) == ids::id(ids::kAttunedShard) && game::userString(item, ids::kNbtAttunement) == uid) {
            found = true;
            ctx.check(!item.getCustomLore().empty(), "the ejected shard has a tooltip");
            a->remove();
        }
    }
    ctx.check(found, "an attuned shard bound to the plate lies next to it");
});

BS_REGISTER_SELFTEST("warpplate.insert_eject", [](Context& ctx) -> ll::coro::CoroTask<> {
    co_await arrive(ctx, 305);
    auto& p   = ctx.player();
    auto  uid = makePlate(ctx, 305);
    p.setPlayerGameType(GameType::Survival);
    auto shard = feature::warpplate::makeAttunedShard("some-target");
    int  s     = ctx.give(p, shard, 0);
    ctx.require(s == 0, "shard given");
    ctx.check(WarpPlates::get().insertShard(p, uid, {game::ItemLocation::Kind::Inventory, 0}), "insert");
    ctx.check(
        slot(uid, 0) && slot(uid, 0)->user.at(std::string(ids::kNbtAttunement)) == "some-target",
        "shard slot holds the bound shard"
    );
    ctx.check(game::itemAt(p, {game::ItemLocation::Kind::Inventory, 0}).isNull(), "survival: the shard left the hand");
    ctx.give(p, feature::warpplate::makeAttunedShard("other"), 0);
    ctx.check(
        !WarpPlates::get().insertShard(p, uid, {game::ItemLocation::Kind::Inventory, 0}),
        "a full shard slot refuses"
    );
    ctx.check(WarpPlates::get().ejectShard(uid), "eject");
    ctx.check(!slot(uid, 0), "shard slot empty after eject");
    p.setPlayerGameType(GameType::Creative);
    ctx.check(WarpPlates::get().insertShard(p, uid, {game::ItemLocation::Kind::Inventory, 0}), "creative insert");
    ctx.check(!game::itemAt(p, {game::ItemLocation::Kind::Inventory, 0}).isNull(), "creative keeps the shard");
});

BS_REGISTER_SELFTEST("warpplate.teleport", [](Context& ctx) -> ll::coro::CoroTask<> {
    co_await arrive(ctx, 310);
    auto& p = ctx.player();
    p.setPlayerGameType(GameType::Survival);
    auto a    = makePlate(ctx, 310);
    auto b    = makePlate(ctx, 320);
    auto aPos = game::Waystones::get().find(a)->pos;
    auto bPos = game::Waystones::get().find(b)->pos;
    setShard(b, ids::id(ids::kAttunedShard), a); // a shard of plate A inside plate B: B warps to A
    co_await ctx.ticks(2);
    ctx.check(plateState(bPos) == static_cast<int>(ids::WarpPlateStatus::Idle), "B is idle with a shard");
    standOn(p, bPos);
    co_await ctx.ticks(4);
    ctx.check(
        plateState(bPos) == static_cast<int>(ids::WarpPlateStatus::Warping),
        "B is warping while the player stands on it"
    );
    co_await ctx.ticks(waitTicks());
    ctx.check(near(p, aPos), "the player arrived at plate A");
    ctx.check(slot(b, 0).has_value(), "a regular attuned shard is not consumed");
});

BS_REGISTER_SELFTEST("warpplate.cooldown", [](Context& ctx) -> ll::coro::CoroTask<> {
    co_await arrive(ctx, 330);
    auto& p = ctx.player();
    p.setPlayerGameType(GameType::Survival);
    auto a    = makePlate(ctx, 330);
    auto b    = makePlate(ctx, 340);
    auto aPos = game::Waystones::get().find(a)->pos;
    auto bPos = game::Waystones::get().find(b)->pos;
    setShard(a, ids::id(ids::kAttunedShard), b);
    setShard(b, ids::id(ids::kAttunedShard), a);
    standOn(p, bPos);
    co_await ctx.ticks(waitTicks());
    ctx.require(near(p, aPos), "warped from B to A");
    ctx.check(WarpPlates::get().onCooldown(a, p.getOrCreateUniqueID().rawID), "arrival marks the cooldown at A");
    standOn(p, aPos); // straight back onto A: the cooldown holds
    co_await ctx.ticks(waitTicks());
    ctx.check(near(p, aPos, 0.9f), "A does not send the player back while on cooldown");
    auto off  = aPos;
    off.z    += 3;
    standOn(p, off);
    co_await ctx.ticks(config::get<feature::warpplate::WarpPlateConfig>().warpPlateCooldownTime + 4);
    ctx.check(!WarpPlates::get().onCooldown(a, p.getOrCreateUniqueID().rawID), "cooldown expired off the plate");
    standOn(p, aPos);
    co_await ctx.ticks(waitTicks());
    ctx.check(near(p, bPos), "after stepping off, A sends the player to B");
});

BS_REGISTER_SELFTEST("warpplate.locked", [](Context& ctx) -> ll::coro::CoroTask<> {
    co_await arrive(ctx, 350);
    auto& p = ctx.player();
    p.setPlayerGameType(GameType::Survival);
    auto a    = makePlate(ctx, 350);
    auto b    = makePlate(ctx, 360);
    auto aPos = game::Waystones::get().find(a)->pos;
    auto bPos = game::Waystones::get().find(b)->pos;
    setShard(a, ids::id(ids::kDeepslateShard), {});
    setShard(a, ids::id(ids::kAttunedShard), b, 1); // even with a target in a modifier slot
    setShard(b, ids::id(ids::kAttunedShard), a);
    co_await ctx.ticks(2);
    ctx.check(plateState(aPos) == static_cast<int>(ids::WarpPlateStatus::Locked), "a deepslate shard locks the plate");
    standOn(p, aPos);
    co_await ctx.ticks(waitTicks());
    ctx.check(near(p, aPos, 0.9f), "a locked plate does not send");
    standOn(p, bPos);
    co_await ctx.ticks(waitTicks());
    ctx.check(near(p, aPos), "a locked plate is still a target");
});

BS_REGISTER_SELFTEST("warpplate.crumbling", [](Context& ctx) -> ll::coro::CoroTask<> {
    co_await arrive(ctx, 370);
    auto& p = ctx.player();
    p.setPlayerGameType(GameType::Survival);
    auto a    = makePlate(ctx, 370);
    auto b    = makePlate(ctx, 380);
    auto aPos = game::Waystones::get().find(a)->pos;
    auto bPos = game::Waystones::get().find(b)->pos;
    setShard(b, ids::id(ids::kCrumblingAttunedShard), a);
    auto lore = feature::warpplate::shardLore(nullptr, ids::id(ids::kCrumblingAttunedShard), a);
    ctx.check(lore.size() == 3 && lore[2].find("consumed") != std::string::npos, "crumbling tooltip line");
    standOn(p, bPos);
    co_await ctx.ticks(waitTicks());
    ctx.check(near(p, aPos), "warped with a crumbling shard");
    ctx.check(!slot(b, 0), "the crumbling shard was consumed");
});

BS_REGISTER_SELFTEST("warpplate.no_target", [](Context& ctx) -> ll::coro::CoroTask<> {
    co_await arrive(ctx, 385);
    auto& p    = ctx.player();
    auto  b    = makePlate(ctx, 385);
    auto  bPos = game::Waystones::get().find(b)->pos;
    setShard(b, ids::id(ids::kAttunedShard), "00000000-dead-4bad-8bad-000000000000");
    auto lore =
        feature::warpplate::shardLore(nullptr, ids::id(ids::kAttunedShard), "00000000-dead-4bad-8bad-000000000000");
    ctx.check(!lore.empty() && lore[0].find("lost its attunement") != std::string::npos, "lost attunement tooltip");
    standOn(p, bPos);
    co_await ctx.ticks(4);
    ctx.check(
        plateState(bPos) == static_cast<int>(ids::WarpPlateStatus::WarpingInvalid),
        "an invalid target shows warping_invalid"
    );
    co_await ctx.ticks(waitTicks());
    ctx.check(near(p, bPos, 0.9f), "no warp to a missing target");
});

BS_REGISTER_SELFTEST("warpplate.use_time", [](Context& ctx) -> ll::coro::CoroTask<> {
    co_await arrive(ctx, 390);
    auto  uid = makePlate(ctx, 390);
    auto& wp  = WarpPlates::get();
    int   cfg = config::get<feature::warpplate::WarpPlateConfig>().warpPlateUseTime;
    ctx.check(wp.useTimeOf(*game::Waystones::get().find(uid)) == cfg, "no modifiers: configured use time");
    setShard(uid, "minecraft:amethyst_shard", {}, 1, 5);
    ctx.check(
        wp.useTimeOf(*game::Waystones::get().find(uid)) == core::warpplate::useTime(cfg, 5, 0),
        "amethyst shards speed up"
    );
    setShard(uid, "minecraft:slime_ball", {}, 2, 64);
    ctx.check(
        wp.useTimeOf(*game::Waystones::get().find(uid)) == core::warpplate::useTime(cfg, 5, 64),
        "slime balls slow down"
    );
    setShard(uid, {}, {}, 1);
    ctx.check(
        wp.useTimeOf(*game::Waystones::get().find(uid)) == std::min(cfg * 2, core::warpplate::useTime(cfg, 0, 64)),
        "clamped to twice"
    );
});

BS_REGISTER_SELFTEST("warpplate.redstone", [](Context& ctx) -> ll::coro::CoroTask<> {
    co_await arrive(ctx, 393);
    auto a   = makePlate(ctx, 393);
    auto pos = game::Waystones::get().find(a)->pos;
    setShard(a, ids::id(ids::kAttunedShard), "x");
    setShard(a, "minecraft:redstone_torch", {}, 1);
    co_await ctx.ticks(2);
    ctx.check(plateState(pos) == static_cast<int>(ids::WarpPlateStatus::Idle), "unpowered: idle");
    auto side  = pos;
    side.x    += 1;
    ctx.setBlock(side, "minecraft:redstone_block");
    co_await ctx.ticks(2);
    ctx.check(
        plateState(pos) == static_cast<int>(ids::WarpPlateStatus::RedstoneDisabled),
        "powered + torch modifier: redstone_disabled"
    );
});

BS_REGISTER_SELFTEST("warpplate.break", [](Context& ctx) -> ll::coro::CoroTask<> {
    co_await arrive(ctx, 396);
    auto&                         ws  = game::Waystones::get();
    auto                          a   = makePlate(ctx, 396, true);
    auto                          pos = ws.find(a)->pos;
    feature::warpplate::BreakInfo silk{false, true, true, 0};
    WarpPlates::get().noteBreak(pos, silk);
    auto* region = &ll::service::getLevel()->getDimension(0).lock()->getBlockSourceFromMainChunkSource();
    game::clearBlock(*region, game::blockPos(pos));
    ctx.check(ws.find(a) && ws.find(a)->transient, "silk touch keeps the record (transient)");
    // Placing the silk-touched item again restores the record.
    ctx.setBlock(pos, feature::warpplate::plateBlockId());
    auto* r = WarpPlates::get().onPlaced(&ctx.player(), pos, a, true);
    ctx.check(r && r->uid == a && !r->transient, "silk-touched item restores the plate");
    WarpPlates::get().noteBreak(pos, {false, true, false, 0});
    game::clearBlock(*region, game::blockPos(pos));
    ctx.check(ws.find(a) == nullptr, "a regular break removes the record");
    co_await ctx.ticks(2);
    // Clean up the dropped items.
    AABB box{
        Vec3{static_cast<float>(pos.x - 2), static_cast<float>(pos.y - 2), static_cast<float>(pos.z - 2)},
        Vec3{static_cast<float>(pos.x + 3), static_cast<float>(pos.y + 3), static_cast<float>(pos.z + 3)}
    };
    int plates = 0;
    for (auto* e : region->getEntities(box, 0.f)) {
        if (!e || !e->hasCategory(ActorCategory::Item)) continue;
        if (game::idOf(static_cast<ItemActor&>(*e).item()) == feature::warpplate::plateBlockId()) ++plates;
        e->remove();
    }
    ctx.check(plates == 2, "both breaks dropped the plate item (" + std::to_string(plates) + ")");
});
