// Selftests of portstones (unit U2, test positions dx 250..299).

#include <algorithm>

#include "mc/scripting/modules/minecraft/ScriptFacing.h"
#include "mc/server/SimulatedPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"

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


} // namespace

// Placing a portstone with the item makes both halves and no record; no Edit screen.
BS_REGISTER_SELFTEST("portstone.place", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto&    p = ctx.player();
    BlockPos ground;
    co_await prepareSite(ctx, p, 250, ground);
    std::vector<std::string>           edits;
    SeamSwap<bs::service::IEditScreen> swap(std::make_unique<EditProbe>(&edits));
    auto&                              region = p.getDimensionBlockSource();
    auto                               id     = bs::ids::id("white_portstone");
    asSim(p).simulateSetItem(bs::game::makeItem(id), true, 0);
    asSim(p).simulateUseItemInSlotOnBlock(
        0,
        BlockPos{ground.x, ground.y - 1, ground.z},
        ScriptFacing::Up,
        Vec3{0.5f, 1.f, 0.5f}
    );
    co_await ctx.ticks(3);
    int facing = bs::game::opposite(bs::game::playerFacing(p));
    ctx.require(bs::game::blockIdAt(region, ground) == id, "the lower half was placed");
    ctx.check(
        bs::game::intState(region.getBlock(ground), bs::ids::kStateFacing).value_or(-1) == facing,
        "faces the player"
    );
    auto const& up = region.getBlock(above(ground));
    ctx.check(
        bs::game::blockId(up) == id && bs::game::intState(up, bs::ids::kStateUpper).value_or(0) == 1,
        "upper half placed"
    );
    ctx.check(
        bs::game::Waystones::get().findAt(bs::game::globalPos(region, ground)) == nullptr,
        "portstones have no record"
    );
    ctx.check(edits.empty(), "no Edit screen for portstones");
    // Breaking it removes both halves.
    bs::game::clearBlock(region, ground);
    co_await ctx.ticks(3);
    ctx.check(bs::game::blockIdAt(region, above(ground)) != id, "the upper half went with the lower one");
});

// White lists the player's activated (+ global) waystones; a colour lists that colour's
// sharestones. Sneaking makes no difference.
BS_REGISTER_SELFTEST("portstone.colour_rule", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto&    p = ctx.player();
    BlockPos ground;
    co_await prepareSite(ctx, p, 260, ground);
    auto&                              region = p.getDimensionBlockSource();
    std::vector<bs::core::WarpSource>  opened;
    std::vector<std::string>           edits;
    SeamSwap<bs::service::ISelection>  s1(std::make_unique<SelectionProbe>(&opened));
    SeamSwap<bs::service::IEditScreen> s2(std::make_unique<EditProbe>(&edits));

    auto rec = [&](int dx, std::string const& type, bs::core::Visibility v, std::string name) {
        bs::core::WaystoneRecord r;
        r.type       = type;
        r.name       = std::move(name);
        r.pos        = ctx.testPos(dx, 5);
        r.visibility = v;
        r.origin     = bs::core::Origin::Player;
        return ctx.createWaystone(r).uid;
    };
    auto red1  = rec(270, "red_sharestone", bs::core::Visibility::Sharestone, "Red A");
    auto red2  = rec(272, "red_sharestone", bs::core::Visibility::Sharestone, "Red B");
    auto blue  = rec(274, "blue_sharestone", bs::core::Visibility::Sharestone, "Blue A");
    auto mine  = rec(276, "waystone", bs::core::Visibility::Activation, "Mine");
    auto other = rec(278, "waystone", bs::core::Visibility::Activation, "Not Mine");
    bs::game::Waystones::get().activate(p, mine);

    auto listFor = [&](bs::core::WarpSource const& src) {
        std::vector<std::string> ids;
        for (auto const& t : bs::service::get<bs::service::ITargets>().list(p, src)) ids.push_back(t.id);
        return ids;
    };
    auto has = [](std::vector<std::string> const& v, std::string const& id) {
        return std::find(v.begin(), v.end(), id) != v.end();
    };

    // White, real right-click (holding a stick: simulated players cannot use an empty hand).
    ctx.require(completePlacement(p, region, ground, {Family::Portstone, "white"}), "placed a white portstone");
    asSim(p).simulateSetItem(bs::game::makeItem("minecraft:stick"), true, 0);
    asSim(p).simulateUseItemInSlotOnBlock(0, above(ground), ScriptFacing::Up, Vec3{0.5f, 1.f, 0.5f});
    co_await ctx.ticks(3);
    ctx.require(
        opened.size() == 1,
        "the white portstone opened a selection menu (got " + std::to_string(opened.size()) + ")"
    );
    auto white = opened.front();
    ctx.check(
        white.kind == bs::core::WarpSourceKind::Portstone && white.portstoneColor == "white",
        "white Portstone source"
    );
    ctx.check(
        white.fromUid.empty() && white.fromPos == bs::game::globalPos(region, ground),
        "no source record, block position"
    );
    auto wl = listFor(white);
    ctx.check(has(wl, mine), "white lists activated waystones");
    ctx.check(!has(wl, other), "white does not list waystones that are not activated");
    ctx.check(!has(wl, red1) && !has(wl, blue), "white does not list sharestones");

    // Red, sneaking: still the selection menu.
    bs::game::clearBlock(region, above(ground));
    co_await ctx.ticks(3);
    ctx.require(completePlacement(p, region, ground, {Family::Portstone, "red"}), "placed a red portstone");
    opened.clear();
    ctx.require(useStone(p, region, ground, true), "used the red portstone while sneaking");
    ctx.require(opened.size() == 1, "the red portstone opened a selection menu");
    auto red = opened.front();
    ctx.check(red.portstoneColor == "red", "red Portstone source");
    auto rl = listFor(red);
    ctx.check(has(rl, red1) && has(rl, red2), "red lists the red sharestones");
    ctx.check(!has(rl, blue) && !has(rl, mine), "red lists nothing else");
    ctx.check(edits.empty(), "portstones never open Edit");
    ctx.check(bs::game::Waystones::get().findAt(bs::game::globalPos(region, ground)) == nullptr, "still no record");
});
