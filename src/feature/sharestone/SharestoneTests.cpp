// Selftests of sharestones (unit U2, test positions dx 200..249).

#include <algorithm>

#include "mc/scripting/modules/minecraft/ScriptFacing.h"
#include "mc/server/SimulatedPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"

#include "feature/sharestone/Sharestones.h"
#include "feature/sharestone/StoneBlocks.h"
#include "feature/sharestone/StoneTestSupport.h"
#include "game/Blocks.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "selftest/SelfTest.h"
#include "service/Services.h"

namespace {

using bs::selftest::Context;
using namespace bs::feature::sharestone;
using namespace bs::feature::sharestone::test;
using ScriptFacing = ScriptModuleMinecraft::ScriptFacing;


bool isHalf(BlockSource& region, BlockPos const& pos, std::string const& id, int upper, int facing) {
    auto const& b = region.getBlock(pos);
    return bs::game::blockId(b) == id && bs::game::intState(b, bs::ids::kStateUpper).value_or(-1) == upper
        && bs::game::intState(b, bs::ids::kStateFacing).value_or(-1) == facing;
}

bs::core::WaystoneRecord sharestoneRecord(Context& ctx, int dx, std::string const& color, std::string name) {
    bs::core::WaystoneRecord r;
    r.type       = bs::core::types::sharestone(color);
    r.style      = color + "_sharestone";
    r.name       = std::move(name);
    r.pos        = ctx.testPos(dx, 5);
    r.visibility = bs::core::Visibility::Sharestone;
    r.origin     = bs::core::Origin::Player;
    return r;
}

} // namespace

// Placing a red sharestone with the item: both halves face the player, a record with the
// sharestone type / visibility / owner is created and the Edit screen opens.
BS_REGISTER_SELFTEST("sharestone.place", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto&    p = ctx.player();
    BlockPos ground;
    co_await prepareSite(ctx, p, 200, ground);
    std::vector<std::string>           edits;
    SeamSwap<bs::service::IEditScreen> swap(std::make_unique<EditProbe>(&edits));
    auto                               id     = bs::ids::id("red_sharestone");
    auto&                              region = p.getDimensionBlockSource();
    auto                               below  = BlockPos{ground.x, ground.y - 1, ground.z};
    auto                               item   = bs::game::makeItem(id);
    asSim(p).simulateSetItem(item, true, 0);
    bool used = asSim(p).simulateUseItemInSlotOnBlock(0, below, ScriptFacing::Up, Vec3{0.5f, 1.f, 0.5f});
    ctx.info(used ? "item used on the ground" : "simulateUseItemInSlotOnBlock returned false");
    co_await ctx.ticks(3);
    int facing = bs::game::opposite(bs::game::playerFacing(p));
    ctx.require(bs::game::blockIdAt(region, ground) == id, "the lower half was placed");
    ctx.check(isHalf(region, ground, id, 0, facing), "lower half faces the player");
    ctx.check(isHalf(region, above(ground), id, 1, facing), "upper half placed above, facing the player");
    auto* r = bs::game::Waystones::get().findAt(bs::game::globalPos(region, ground));
    ctx.require(r != nullptr, "a record exists at the lower half");
    ctx.cleanupWaystone(r->uid);
    ctx.check(r->type == "red_sharestone", "record type is red_sharestone");
    ctx.check(r->visibility == bs::core::Visibility::Sharestone, "visibility is Sharestone");
    ctx.check(r->origin == bs::core::Origin::Player, "origin is PLAYER");
    ctx.check(r->ownerUid == bs::game::Waystones::playerKey(p), "the placer owns it");
    ctx.check(r->facing == facing, "record keeps the facing");
    ctx.check(!bs::game::Waystones::get().isActivated(p, r->uid), "sharestones are not activated");
    ctx.check(edits.size() == 1 && edits.front() == r->uid, "the Edit screen opened for the new record");
});

// No room above: the placement is taken back and the item returned.
BS_REGISTER_SELFTEST("sharestone.place_blocked", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto&    p = ctx.player();
    BlockPos ground;
    co_await prepareSite(ctx, p, 204, ground);
    auto& region = p.getDimensionBlockSource();
    ctx.setBlock(bs::game::globalPos(region, above(ground)), "minecraft:stone");
    auto id = bs::ids::id("blue_sharestone");
    asSim(p).simulateSetItem(bs::game::makeItem(id), true, 0);
    asSim(p).simulateUseItemInSlotOnBlock(
        0,
        BlockPos{ground.x, ground.y - 1, ground.z},
        ScriptFacing::Up,
        Vec3{0.5f, 1.f, 0.5f}
    );
    co_await ctx.ticks(3);
    ctx.check(bs::game::blockIdAt(region, ground) != id, "the half without room above was removed");
    ctx.check(bs::game::Waystones::get().findAt(bs::game::globalPos(region, ground)) == nullptr, "no record");
    if (!p.isCreative()) {
        bool back = bs::game::findItem(p, [&](ItemStack const& s) { return bs::game::idOf(s) == id; }).has_value();
        ctx.check(back, "the item was given back");
    }
});

// Right-click opens the selection through ISelection with a Sharestone source; the default
// ITargets lists exactly the sharestones of the same colour. Shift + right-click opens Edit.
BS_REGISTER_SELFTEST("sharestone.same_colour", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto&    p = ctx.player();
    BlockPos ground;
    co_await prepareSite(ctx, p, 208, ground);
    auto&                              region = p.getDimensionBlockSource();
    std::vector<bs::core::WarpSource>  opened;
    std::vector<std::string>           edits;
    SeamSwap<bs::service::ISelection>  s1(std::make_unique<SelectionProbe>(&opened));
    SeamSwap<bs::service::IEditScreen> s2(std::make_unique<EditProbe>(&edits));
    ctx.require(completePlacement(p, region, ground, {Family::Sharestone, "red"}), "placed a red sharestone");
    auto* here = bs::game::Waystones::get().findAt(bs::game::globalPos(region, ground));
    ctx.require(here != nullptr, "record of the placed sharestone");
    auto hereUid = here->uid;
    ctx.cleanupWaystone(hereUid);
    edits.clear();
    auto                     redUid  = ctx.createWaystone(sharestoneRecord(ctx, 220, "red", "Red Two")).uid;
    auto                     blueUid = ctx.createWaystone(sharestoneRecord(ctx, 222, "blue", "Blue One")).uid;
    bs::core::WaystoneRecord w;
    w.name    = "Plain";
    w.pos     = ctx.testPos(224, 5);
    w.origin  = bs::core::Origin::Player;
    auto wUid = ctx.createWaystone(w).uid;
    bs::game::Waystones::get().activate(p, wUid);

    // Real right-click on the upper half (simulated players cannot use an empty hand: a stick,
    // which has no use on blocks, takes the same path).
    asSim(p).simulateSetItem(bs::game::makeItem("minecraft:stick"), true, 0);
    bool interacted = asSim(p).simulateUseItemInSlotOnBlock(0, above(ground), ScriptFacing::Up, Vec3{0.5f, 1.f, 0.5f});
    ctx.info(interacted ? "interacted" : "use on block returned false");
    co_await ctx.ticks(3);
    ctx.require(opened.size() == 1, "one selection menu opened (got " + std::to_string(opened.size()) + ")");
    auto const& src = opened.front();
    ctx.check(src.kind == bs::core::WarpSourceKind::Sharestone, "Sharestone source");
    ctx.check(src.fromUid == hereUid, "source is the clicked sharestone");
    ctx.check(src.fromPos == bs::game::globalPos(region, ground), "source position is the lower half");
    ctx.check(edits.empty(), "no Edit screen on a plain right-click");

    auto list = bs::service::get<bs::service::ITargets>().list(p, src);
    auto has  = [&](std::string const& uid) {
        return std::any_of(list.begin(), list.end(), [&](auto const& t) { return t.id == uid; });
    };
    ctx.check(has(redUid), "the other red sharestone is listed");
    ctx.check(has(hereUid), "the source sharestone is listed (as current)");
    ctx.check(!has(blueUid), "a blue sharestone is not listed");
    ctx.check(!has(wUid), "activated waystones are not listed");
    for (auto const& t : list) {
        if (t.id == hereUid) ctx.check(t.current, "the source entry is flagged current");
    }

    // Sneaking with an item in the hand: the block is skipped (Java: the item is used instead).
    asSim(p).simulateSneaking();
    co_await ctx.ticks(10); // past the use cooldown
    asSim(p).simulateUseItemInSlotOnBlock(0, above(ground), ScriptFacing::Up, Vec3{0.5f, 1.f, 0.5f});
    co_await ctx.ticks(3);
    asSim(p).simulateStopSneaking();
    ctx.check(edits.empty() && opened.size() == 1, "sneaking with an item does not use the sharestone");
    // Sneaking with an empty hand: Edit.
    ctx.require(useStone(p, region, ground, true), "used while sneaking");
    ctx.check(edits.size() == 1 && edits.front() == hereUid, "shift + right-click opens Edit");
    ctx.check(opened.size() == 1, "no selection menu while sneaking");
});

// A sharestone set by a command has no record: using it creates one (no owner).
BS_REGISTER_SELFTEST("sharestone.lazy_record", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto&    p = ctx.player();
    BlockPos ground;
    co_await prepareSite(ctx, p, 212, ground);
    auto& region = p.getDimensionBlockSource();
    auto  gp     = bs::game::globalPos(region, ground);
    ctx.setBlock(
        gp,
        "bedstones:lime_sharestone",
        {
            {"bedstones:upper",  0},
            {"bedstones:facing", 2}
    }
    );
    ctx.setBlock(
        bs::game::globalPos(region, above(ground)),
        "bedstones:lime_sharestone",
        {
            {"bedstones:upper",  1},
            {"bedstones:facing", 2}
    }
    );
    std::vector<bs::core::WarpSource> opened;
    SeamSwap<bs::service::ISelection> swap(std::make_unique<SelectionProbe>(&opened));
    ctx.require(useStone(p, region, above(ground), false), "useStone found the stone");
    auto* r = bs::game::Waystones::get().findAt(gp);
    ctx.require(r != nullptr, "a record was created");
    ctx.cleanupWaystone(r->uid);
    ctx.check(r->type == "lime_sharestone" && r->visibility == bs::core::Visibility::Sharestone, "type and visibility");
    ctx.check(!r->hasOwner() && r->facing == 2, "no owner, facing from the block");
    ctx.check(opened.size() == 1 && opened.front().fromUid == r->uid, "selection opened for it");
    co_return;
});

// Breaking either half removes both and the record; a survival pickaxe break drops one item.
BS_REGISTER_SELFTEST("sharestone.break", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto&    p = ctx.player();
    BlockPos ground;
    co_await prepareSite(ctx, p, 216, ground);
    auto&                              region = p.getDimensionBlockSource();
    std::vector<std::string>           edits;
    SeamSwap<bs::service::IEditScreen> swap(std::make_unique<EditProbe>(&edits));
    auto                               gp = bs::game::globalPos(region, ground);
    auto                               id = bs::ids::id("cyan_sharestone");

    // 1. The upper half disappears (command / any removal): no drop.
    ctx.require(completePlacement(p, region, ground, {Family::Sharestone, "cyan"}), "placed");
    auto* r = bs::game::Waystones::get().findAt(gp);
    ctx.require(r != nullptr, "record created");
    auto uid   = r->uid;
    int  drops = dropCount();
    bs::game::clearBlock(region, above(ground));
    co_await ctx.ticks(3);
    ctx.check(bs::game::blockIdAt(region, ground) != id, "the lower half went with the upper one");
    ctx.check(bs::game::Waystones::get().find(uid) == nullptr, "the record was removed");
    ctx.check(dropCount() == drops, "no drop without a player break");

    // 2. A survival player mines the upper half with a pickaxe: one item drops.
    ctx.require(completePlacement(p, region, ground, {Family::Sharestone, "cyan"}), "placed again");
    r = bs::game::Waystones::get().findAt(gp);
    ctx.require(r != nullptr, "record created again");
    uid          = r->uid;
    auto pickaxe = bs::game::makeItem("minecraft:diamond_pickaxe");
    ctx.check(breakDrops(p, pickaxe) == !p.isCreative(), "pickaxe breaks drop in survival");
    ctx.check(!breakDrops(p, ItemStack::EMPTY_ITEM()), "hand breaks do not drop");
    asSim(p).simulateSetItem(pickaxe, true, 0);
    drops = dropCount();
    // Mine the upper half (hardness 5: about a second with a diamond pickaxe).
    bool started = asSim(p).simulateDestroyBlock(above(ground), ScriptFacing::South);
    ctx.info(started ? "mining started" : "simulateDestroyBlock returned false");
    for (int i = 0; i < 100 && bs::game::blockIdAt(region, above(ground)) == id; ++i) co_await ctx.ticks(2);
    asSim(p).simulateStopDestroyingBlock();
    co_await ctx.ticks(3);
    ctx.check(
        bs::game::blockIdAt(region, ground) != id && bs::game::blockIdAt(region, above(ground)) != id,
        "both halves are gone"
    );
    ctx.check(bs::game::Waystones::get().find(uid) == nullptr, "the record was removed");
    if (!p.isCreative()) ctx.check(dropCount() == drops + 1, "exactly one item dropped");
});
