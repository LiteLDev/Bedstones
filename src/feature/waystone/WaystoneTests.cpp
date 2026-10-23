// Selftests of U1 (waystone blocks): `/waystones selftest "waystone.*"`. Test positions use dx 100..127
// (x >= 128 is outside the loaded spawn chunks), dz 0 and 6.

#include <algorithm>

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/scripting/modules/minecraft/ScriptFacing.h"
#include "mc/server/SimulatedPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/enchanting/Enchant.h"
#include "mc/world/item/enchanting/EnchantUtils.h"
#include "mc/world/item/enchanting/EnchantmentInstance.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/GameType.h"

#include "feature/waystone/WaystoneBlocks.h"
#include "game/BlockContainer.h"
#include "game/Blocks.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "selftest/SelfTest.h"
#include "service/Services.h"

namespace {

using bs::selftest::Context;
namespace ws = bs::feature::waystone;
namespace game = bs::game;
namespace ids = bs::ids;

BlockSource& region(Player& p) { return p.getDimensionBlockSource(); }

/// Clears a two-block-tall column with solid ground below; registers everything for cleanup. The
/// test player is moved next to it first (our test area is outside the spawn chunks).
ll::coro::CoroTask<BlockPos> prepare(Context& ctx, int dx, int dz = 0) {
    auto  pos = ctx.testPos(dx, dz);
    auto& p   = ctx.player();
    auto  bp  = game::blockPos(pos);
    auto  near = [&](float y) { return Vec3{static_cast<float>(pos.x) + 0.5f, y, static_cast<float>(pos.z) + 4.5f}; };
    if (!region(p).hasChunksAt(bp, 2, false) || std::abs(p.getPosition().x - near(0).x) > 8.f) {
        p.teleport(near(p.getPosition().y), p.getDimensionId(), Vec2{0.f, 180.f});
    }
    for (int i = 0; i < 100 && !region(p).hasChunksAt(bp, 2, false); ++i) co_await ctx.ticks(1);
    // The world spawn's y need not be the surface: stand on the ground of the test column.
    pos.y = region(p).getAboveTopSolidBlock(pos.x, pos.z, false, false);
    bp    = game::blockPos(pos);
    if (std::abs(p.getPosition().y - static_cast<float>(pos.y)) > 1.f) p.teleport(near(static_cast<float>(pos.y)), p.getDimensionId(), Vec2{0.f, 180.f});
    ctx.setBlock({pos.dim, pos.x, pos.y - 1, pos.z}, "minecraft:stone");
    ctx.setBlock(pos, "minecraft:air");
    ctx.setBlock({pos.dim, pos.x, pos.y + 1, pos.z}, "minecraft:air");
    ctx.require(game::blockIdAt(region(p), BlockPos{pos.x, pos.y - 1, pos.z}) == "minecraft:stone", "test area is loaded");
    co_return bp;
}

/// Places a waystone through the C++ helper (no Edit screen) and registers its record for cleanup.
ws::Placed placeAt(Context& ctx, Player& p, BlockPos const& pos, ItemStack const& item, int facing = 2) {
    auto res = ws::place(p, region(p), pos, item, facing, false);
    if (!res.uid.empty()) ctx.cleanupWaystone(res.uid);
    return res;
}

ItemStack waystoneItem(std::string_view path = "waystone") { return game::makeItem(ids::id(path)); }

ItemStack pickaxe(bool silk) {
    auto stack = game::makeItem("minecraft:diamond_pickaxe");
    if (silk) {
        EnchantmentInstance e;
        e.mEnchantType = Enchant::Type::SilkTouch;
        e.mLevel       = 1;
        EnchantUtils::applyEnchant(stack, e, true);
    }
    return stack;
}

void hold(Context& ctx, Player& p, ItemStack const& item) { ctx.give(p, item, p.getSelectedItemSlot()); }

/// Swaps a seam for a probe for the duration of a test (the unit owning it may have installed one).
template <class I>
struct SeamSwap {
    std::unique_ptr<I> saved;
    explicit SeamSwap(std::unique_ptr<I> probe) {
        saved                                 = std::move(bs::service::detail::slot<I>());
        bs::service::detail::slot<I>() = std::move(probe);
    }
    ~SeamSwap() { bs::service::detail::slot<I>() = std::move(saved); }
};

struct EditProbe final : bs::service::IEditScreen {
    std::vector<std::string>* calls;
    explicit EditProbe(std::vector<std::string>* c) : calls(c) {}
    void open(Player&, std::string const& uid) override { calls->push_back(uid); }
};

struct SelectionProbe final : bs::service::ISelection {
    std::vector<bs::core::WarpSource>* calls;
    explicit SelectionProbe(std::vector<bs::core::WarpSource>* c) : calls(c) {}
    void open(Player&, bs::core::WarpSource const& s) override { calls->push_back(s); }
    void openAdmin(Player&, std::string const&) override {}
};

} // namespace

BS_REGISTER_SELFTEST("waystone.place", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = ctx.player();
    auto  pos = co_await prepare(ctx, 100);
    auto  res = placeAt(ctx, p, pos, waystoneItem("mossy_waystone"), 3);
    ctx.require(res.result == ws::PlaceResult::Ok, "placement succeeded");
    auto& r     = region(p);
    auto  upper = BlockPos{pos.x, pos.y + 1, pos.z};
    ctx.check(game::blockIdAt(r, pos) == "bedstones:mossy_waystone", "lower half placed");
    ctx.check(game::blockIdAt(r, upper) == "bedstones:mossy_waystone", "upper half placed");
    ctx.check(game::intState(r.getBlock(pos), ids::kStateUpper) == 0, "lower half has upper=0");
    ctx.check(game::intState(r.getBlock(upper), ids::kStateUpper) == 1, "upper half has upper=1");
    ctx.check(game::intState(r.getBlock(pos), ids::kStateFacing) == 3, "lower facing");
    ctx.check(game::intState(r.getBlock(upper), ids::kStateFacing) == 3, "upper facing");
    auto* rec = game::Waystones::get().find(res.uid);
    ctx.require(rec != nullptr, "record created");
    ctx.check(rec->pos == game::globalPos(r, pos), "record at the lower half");
    ctx.check(rec->isWaystone() && rec->style == "mossy_waystone", "type waystone, style = block path");
    ctx.check(rec->origin == bs::core::Origin::Player, "origin PLAYER");
    ctx.check(rec->ownerUid == game::Waystones::playerKey(p) && rec->ownerName == p.getRealName(), "owner = placer");
    ctx.check(rec->visibility == bs::service::get<bs::service::IPermissions>().defaultVisibility(), "default visibility");
    ctx.check(rec->name.empty(), "player-placed waystones are not named");
    ctx.check(rec->facing == 3 && !rec->transient, "facing stored, not transient");
    ctx.check(game::Waystones::get().isActivated(p, res.uid), "activated for the placer");
    ctx.check(ws::recordAt(r, upper, false) == rec, "the upper half resolves to the same record");
});

BS_REGISTER_SELFTEST("waystone.facing_player", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = ctx.player();
    auto  pos = co_await prepare(ctx, 103);
    auto  res = ws::place(p, region(p), pos, waystoneItem(), std::nullopt, false);
    if (!res.uid.empty()) ctx.cleanupWaystone(res.uid);
    ctx.require(res.result == ws::PlaceResult::Ok, "placement succeeded");
    int expected = game::opposite(game::playerFacing(p));
    ctx.check(game::intState(region(p).getBlock(pos), ids::kStateFacing) == expected, "faces the player");
});

BS_REGISTER_SELFTEST("waystone.no_room", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = ctx.player();
    auto  pos = co_await prepare(ctx, 106);
    auto  g   = game::globalPos(region(p), pos);
    ctx.setBlock({g.dim, g.x, g.y + 1, g.z}, "minecraft:stone");
    auto res = placeAt(ctx, p, pos, waystoneItem());
    ctx.check(res.result == ws::PlaceResult::NoRoom, "refused without room above");
    ctx.check(game::blockIdAt(region(p), pos) != "bedstones:waystone", "nothing placed");
    ctx.check(game::Waystones::get().findAt(g) == nullptr, "no record");
});

BS_REGISTER_SELFTEST("waystone.two_tall", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = ctx.player();
    auto  pos = co_await prepare(ctx, 109);
    auto  res = placeAt(ctx, p, pos, waystoneItem());
    ctx.require(res.result == ws::PlaceResult::Ok, "placement succeeded");
    auto upper = BlockPos{pos.x, pos.y + 1, pos.z};
    game::clearBlock(region(p), upper); // e.g. /setblock air on the top
    co_await ctx.ticks(2);
    ctx.check(game::blockIdAt(region(p), pos) == "minecraft:air", "the lower half went with the upper one");
    ctx.check(game::Waystones::get().find(res.uid) == nullptr, "record removed");
    ctx.check(!game::Waystones::get().playerData(p).isActivated(res.uid), "players forget it");
});

BS_REGISTER_SELFTEST("waystone.activation", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& owner = ctx.player();
    auto& other = ctx.spawnPlayer();
    auto  pos   = co_await prepare(ctx, 112);
    auto  res   = placeAt(ctx, owner, pos, waystoneItem());
    ctx.require(res.result == ws::PlaceResult::Ok, "placement succeeded");
    auto& data = game::Waystones::get();
    ctx.check(!data.isActivated(other, res.uid), "not activated for another player");
    std::vector<bs::core::WarpSource> selections;
    SeamSwap<bs::service::ISelection> swap(std::make_unique<SelectionProbe>(&selections));
    ws::use(other, region(other), pos, false, "");
    ctx.check(data.isActivated(other, res.uid), "right-click activates");
    ctx.check(selections.empty(), "the first click does not open the menu");
    auto* rec = data.find(res.uid);
    ctx.require(rec != nullptr, "record kept");
    ctx.check(rec->name.empty(), "player-placed waystones stay unnamed");
    ctx.check(rec->ownerUid == game::Waystones::playerKey(owner), "owner unchanged");
    ws::use(other, region(other), BlockPos{pos.x, pos.y + 1, pos.z}, false, "");
    ctx.require(selections.size() == 1, "second click (upper half) opens the selection menu");
    ctx.check(selections[0].kind == bs::core::WarpSourceKind::Waystone, "source kind waystone");
    ctx.check(selections[0].fromUid == res.uid, "source uid");
    ctx.check(selections[0].fromPos == rec->pos, "source position");
});

BS_REGISTER_SELFTEST("waystone.first_activation_names", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = ctx.player();
    auto  pos = co_await prepare(ctx, 115);
    auto  g   = game::globalPos(region(p), pos);
    ctx.setBlock(g, "bedstones:waystone", {{std::string(ids::kStateFacing), 0}, {std::string(ids::kStateUpper), 0}});
    ctx.setBlock({g.dim, g.x, g.y + 1, g.z}, "bedstones:waystone", {{std::string(ids::kStateFacing), 0}, {std::string(ids::kStateUpper), 1}});
    bs::core::WaystoneRecord wild;
    wild.pos    = g;
    wild.style  = "waystone";
    wild.origin = bs::core::Origin::Wilderness;
    auto& rec   = ctx.createWaystone(wild);
    auto  uid   = rec.uid;
    ws::use(p, region(p), pos, false, "");
    auto* after = game::Waystones::get().find(uid);
    ctx.require(after != nullptr, "record kept");
    ctx.check(!after->name.empty(), "generated waystone named on first activation");
    ctx.check(after->ownerUid == game::Waystones::playerKey(p), "activator becomes the owner");
    ctx.check(after->visibility == bs::service::get<bs::service::IPermissions>().defaultVisibility(), "default visibility");
    ctx.check(game::Waystones::get().isActivated(p, uid), "activated");
    // A waystone block without any record (e.g. set by a command) gets one on first use.
    auto pos2 = co_await prepare(ctx, 118);
    auto g2   = game::globalPos(region(p), pos2);
    ctx.setBlock(g2, "bedstones:sandy_waystone", {{std::string(ids::kStateFacing), 1}, {std::string(ids::kStateUpper), 0}});
    ctx.setBlock({g2.dim, g2.x, g2.y + 1, g2.z}, "bedstones:sandy_waystone", {{std::string(ids::kStateFacing), 1}, {std::string(ids::kStateUpper), 1}});
    ws::use(p, region(p), pos2, false, "");
    auto* orphan = game::Waystones::get().findAt(g2);
    ctx.require(orphan != nullptr, "orphan block got a record");
    ctx.cleanupWaystone(orphan->uid);
    ctx.check(orphan->style == "sandy_waystone" && orphan->facing == 1, "style and facing from the block");
    ctx.check(game::Waystones::get().isActivated(p, orphan->uid), "and was activated");
});

BS_REGISTER_SELFTEST("waystone.shift_opens_edit", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = ctx.player();
    auto  pos = co_await prepare(ctx, 121);
    auto  res = placeAt(ctx, p, pos, waystoneItem());
    ctx.require(res.result == ws::PlaceResult::Ok, "placement succeeded");
    auto                     pos2 = co_await prepare(ctx, 124);
    std::vector<std::string> edits;
    {
        SeamSwap<bs::service::IEditScreen> swap(std::make_unique<EditProbe>(&edits));
        ws::use(p, region(p), pos, true, "");
        // Placing through the helper with openEdit opens it as well.
        auto res2 = ws::place(p, region(p), pos2, waystoneItem(), 0, true);
        if (!res2.uid.empty()) ctx.cleanupWaystone(res2.uid);
        ctx.require(edits.size() == 2, "edit opened by shift-use and by placement");
        ctx.check(edits[1] == res2.uid, "placement opens the new waystone");
    }
    ctx.check(edits[0] == res.uid, "edit opened for the clicked waystone");
});

BS_REGISTER_SELFTEST("waystone.break_plain", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p   = ctx.player();
    auto& sp  = static_cast<SimulatedPlayer&>(p);
    sp.setPlayerGameType(GameType::Survival);
    auto pos = co_await prepare(ctx, 127);
    auto res = placeAt(ctx, p, pos, waystoneItem("deepslate_waystone"));
    ctx.require(res.result == ws::PlaceResult::Ok, "placement succeeded");
    auto* rec = game::Waystones::get().find(res.uid);
    ctx.require(rec != nullptr, "record");
    auto c = game::containerOf(*rec);
    c[1]   = game::SlotItem{"minecraft:diamond", 2, 0, {}};
    game::setContainer(*rec, c);
    hold(ctx, p, pickaxe(false));
    ws::noteBreak(p, region(p), BlockPos{pos.x, pos.y + 1, pos.z}); // breaking the upper half
    game::clearBlock(region(p), BlockPos{pos.x, pos.y + 1, pos.z});
    co_await ctx.ticks(2);
    ctx.check(game::blockIdAt(region(p), pos) == "minecraft:air", "both halves removed");
    ctx.check(game::Waystones::get().find(res.uid) == nullptr, "record deleted without silk touch");
    ctx.check(!game::Waystones::get().playerData(p).isActivated(res.uid), "forgotten");
    auto const& drops = ws::lastDrops();
    int waystones = 0, diamonds = 0;
    for (auto const& d : drops) {
        if (game::idOf(d) == "bedstones:deepslate_waystone") {
            ++waystones;
            ctx.check(!game::userString(d, ids::kNbtWaystone), "plain drop carries no uid");
        }
        if (game::idOf(d) == "minecraft:diamond") diamonds += d.mCount;
    }
    ctx.check(waystones == 1, "exactly one waystone item dropped");
    ctx.check(diamonds == 2, "container contents dropped");
});

BS_REGISTER_SELFTEST("waystone.break_no_tool", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p = ctx.player();
    static_cast<SimulatedPlayer&>(p).setPlayerGameType(GameType::Survival);
    auto pos = co_await prepare(ctx, 101, 6);
    auto res = placeAt(ctx, p, pos, waystoneItem());
    ctx.require(res.result == ws::PlaceResult::Ok, "placement succeeded");
    hold(ctx, p, game::makeItem("minecraft:stick"));
    ws::noteBreak(p, region(p), pos);
    game::clearBlock(region(p), pos);
    co_await ctx.ticks(2);
    ctx.check(game::blockIdAt(region(p), BlockPos{pos.x, pos.y + 1, pos.z}) == "minecraft:air", "upper half removed");
    ctx.check(ws::lastDrops().empty(), "no drop without a pickaxe");
    ctx.check(game::Waystones::get().find(res.uid) == nullptr, "record deleted");
});

BS_REGISTER_SELFTEST("waystone.break_silk_and_restore", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p     = ctx.player();
    auto& other = ctx.spawnPlayer();
    static_cast<SimulatedPlayer&>(p).setPlayerGameType(GameType::Survival);
    auto pos = co_await prepare(ctx, 104, 6);
    auto res = placeAt(ctx, p, pos, waystoneItem("purpur_waystone"));
    ctx.require(res.result == ws::PlaceResult::Ok, "placement succeeded");
    auto uid = res.uid;
    {
        auto* rec = game::Waystones::get().find(uid);
        rec->name = "Silky";
        game::Waystones::get().markDirty(uid);
    }
    game::Waystones::get().activate(other, uid);
    hold(ctx, p, pickaxe(true));
    ws::noteBreak(p, region(p), pos);
    game::clearBlock(region(p), pos);
    co_await ctx.ticks(2);
    auto* kept = game::Waystones::get().find(uid);
    ctx.require(kept != nullptr, "record kept with silk touch");
    ctx.check(kept->transient, "marked transient");
    ctx.check(game::blockIdAt(region(p), BlockPos{pos.x, pos.y + 1, pos.z}) == "minecraft:air", "both halves removed");
    std::optional<ItemStack> item;
    for (auto const& d : ws::lastDrops()) {
        if (game::idOf(d) == "bedstones:purpur_waystone") item = d;
    }
    ctx.require(item.has_value(), "silk-touched waystone item dropped");
    ctx.check(game::userString(*item, ids::kNbtWaystone) == uid, "item carries the waystone uid");
    auto lore = item->getCustomLore();
    ctx.check(!lore.empty() && lore[0] == "§bSilky", "item shows the name in aqua");
    // Another waystone placed where it stood is a new one; the silk-touched record stays in transit.
    auto fresh = placeAt(ctx, p, pos, waystoneItem());
    ctx.require(fresh.result == ws::PlaceResult::Ok && !fresh.restored && fresh.uid != uid, "fresh waystone at the old spot");
    ctx.check(game::Waystones::get().find(uid) && game::Waystones::get().find(uid)->transient, "silk record still transient");
    // Placed again elsewhere: the record comes back.
    auto pos2 = co_await prepare(ctx, 107, 6);
    auto back = placeAt(ctx, p, pos2, *item, 1);
    ctx.require(back.result == ws::PlaceResult::Ok, "re-placement succeeded");
    ctx.check(back.restored && back.uid == uid, "the same record is restored");
    auto* r = game::Waystones::get().find(uid);
    ctx.require(r != nullptr, "record");
    ctx.check(!r->transient, "no longer transient");
    ctx.check(r->pos == game::globalPos(region(p), pos2), "moved to the new position");
    ctx.check(r->name == "Silky" && r->ownerUid == game::Waystones::playerKey(p), "name and owner kept");
    ctx.check(game::Waystones::get().isActivated(other, uid), "activations kept");
    ctx.check(r->facing == 1, "new facing");
    auto* atOld = game::Waystones::get().findAt(game::globalPos(region(p), pos));
    ctx.check(atOld && atOld->uid == fresh.uid, "the waystone at the old spot keeps its record");
    ctx.check(ws::recordAt(region(p), pos, false) == atOld, "and its block resolves to it");
});

BS_REGISTER_SELFTEST("waystone.creative", [](Context& ctx) -> ll::coro::CoroTask<> {
    auto& p = ctx.player();
    static_cast<SimulatedPlayer&>(p).setPlayerGameType(GameType::Creative);
    auto pos = co_await prepare(ctx, 110, 6);
    auto res = placeAt(ctx, p, pos, waystoneItem());
    ctx.require(res.result == ws::PlaceResult::Ok, "placement succeeded");
    // Debug: the bamboo resets the waystone (a fresh record at the same place).
    ws::use(p, region(p), pos, false, "minecraft:bamboo");
    auto* fresh = game::Waystones::get().findAt(game::globalPos(region(p), pos));
    ctx.require(fresh != nullptr, "re-initialized record");
    ctx.cleanupWaystone(fresh->uid);
    ctx.check(fresh->uid != res.uid && game::Waystones::get().find(res.uid) == nullptr, "new uid after reset");
    ctx.check(!fresh->hasOwner() && fresh->name.empty(), "reset clears owner and name");
    auto freshUid = fresh->uid;
    // Creative breaking drops nothing, even with silk touch, and deletes the record.
    hold(ctx, p, pickaxe(true));
    ws::noteBreak(p, region(p), pos);
    game::clearBlock(region(p), pos);
    co_await ctx.ticks(2);
    ctx.check(ws::lastDrops().empty(), "creative drops nothing");
    ctx.check(game::Waystones::get().find(freshUid) == nullptr, "record deleted");
});

BS_REGISTER_SELFTEST("waystone.event_place", [](Context& ctx) -> ll::coro::CoroTask<> {
    // Places a waystone the way a player does (use the item on the ground), through the interact
    // event and the deferred placement.
    auto& p  = ctx.player();
    auto& sp = static_cast<SimulatedPlayer&>(p);
    sp.setPlayerGameType(GameType::Survival);
    auto pos    = co_await prepare(ctx, 113, 6);
    auto ground = BlockPos{pos.x, pos.y - 1, pos.z};
    sp.teleport(Vec3{static_cast<float>(pos.x) + 0.5f, static_cast<float>(pos.y), static_cast<float>(pos.z) + 3.5f}, p.getDimensionId());
    co_await ctx.ticks(2);
    int slot = p.getSelectedItemSlot();
    ctx.give(p, game::makeItem("bedstones:waystone", 2), slot);
    sp.simulateUseItemInSlotOnBlock(slot, ground, ScriptModuleMinecraft::ScriptFacing::Up, Vec3{0.5f, 1.f, 0.5f});
    co_await ctx.ticks(3);
    auto* rec = game::Waystones::get().findAt(game::globalPos(region(p), pos));
    ctx.require(rec != nullptr, "record created by the placement");
    ctx.cleanupWaystone(rec->uid);
    ctx.cleanupBlock(game::globalPos(region(p), BlockPos{pos.x, pos.y + 1, pos.z}));
    ctx.check(game::blockIdAt(region(p), BlockPos{pos.x, pos.y + 1, pos.z}) == "bedstones:waystone", "upper half placed");
    ctx.check(p.getSelectedItem().mCount == 1, "one item consumed in survival");
});
