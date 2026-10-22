#include "feature/sharestone/StoneBlocks.h"

#include <cstdint>
#include <map>
#include <tuple>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/player/PlayerDestroyBlockEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "ll/api/event/player/PlayerPlaceBlockEvent.h"
#include "ll/api/event/world/BlockChangedEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/string/HashedString.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ItemTag.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/Spawner.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/dimension/Dimension.h"

#include "feature/portstone/Portstones.h"
#include "feature/sharestone/Sharestones.h"
#include "game/Blocks.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace bs::feature::sharestone {

namespace {

constexpr std::int64_t kUseCooldownTicks = 8; ///< holding "use" repeats the interact event

int gDrops = 0;

BlockSource* regionOf(int dim) {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    auto dimension = level->getDimension(dim).lock();
    if (!dimension) return nullptr;
    return &dimension->getBlockSourceFromMainChunkSource();
}

Player* playerById(std::int64_t id) {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    ActorUniqueID uid;
    uid.rawID = id;
    return level->getPlayer(uid);
}

using Key = std::tuple<int, int, int, int>; ///< dim, x, y, z
Key keyOf(int dim, BlockPos const& p) { return {dim, p.x, p.y, p.z}; }

BlockPos above(BlockPos const& p) { return BlockPos{p.x, p.y + 1, p.z}; }

StoneAt makeStone(StoneKind const& kind, int dim, BlockPos const& lower, int facing) {
    StoneAt s;
    s.kind   = kind;
    s.id     = ids::id(core::sharestone::path(kind));
    s.dim    = dim;
    s.x      = lower.x;
    s.y      = lower.y;
    s.z      = lower.z;
    s.facing = facing & 3;
    return s;
}

/// Cheap first test for the hot block-changed path: one of our ids at all?
bool ourNamespace(std::string const& typeName) {
    return typeName.size() > ids::kNamespace.size() && typeName.starts_with(ids::kNamespace)
        && typeName[ids::kNamespace.size()] == ':';
}

} // namespace

std::optional<StoneKind> stoneKindOf(std::string_view blockId) {
    auto path = ids::pathOf(blockId);
    if (path.empty()) return std::nullopt;
    return core::sharestone::parse(path);
}

std::optional<StoneAt> stoneAt(BlockSource& region, BlockPos const& pos) {
    if (!region.hasChunksAt(pos, 0, false)) return std::nullopt;
    auto const& block = region.getBlock(pos);
    auto        kind  = stoneKindOf(block.getTypeName());
    if (!kind) return std::nullopt;
    auto lower = game::lowerHalf(region, pos);
    return makeStone(
        *kind,
        static_cast<int>(region.getDimensionId()),
        lower,
        game::intState(block, ids::kStateFacing).value_or(0)
    );
}

int dropCount() { return gDrops; }

bool breakDrops(Player const& player, ItemStack const& tool) {
    if (player.isCreative()) return false;
    if (tool.isNull()) return false;
    ItemTag pickaxe{HashedString("minecraft:is_pickaxe")};
    return tool.hasTag(pickaxe);
}

// ---- the module --------------------------------------------------------------------------------

class StoneBlocks {
public:
    static StoneBlocks& get() {
        static StoneBlocks instance;
        return instance;
    }

    void install();
    void uninstall();

    /// Our own block edits (completing / reverting a placement, removing the other half): the
    /// block-changed handler ignores them.
    int guard = 0;

private:
    struct Placed {
        std::int64_t player;
        Placer       placer;
        bool         refund; ///< give the item back if the placement is refused (not creative)
        int          dim;
        BlockPos     pos;
        int          facing; ///< taken when the block was placed
    };
    struct Used {
        std::int64_t player;
        int          dim;
        BlockPos     pos;
        bool         sneaking;
    };
    struct Gone {
        int         dim;
        BlockPos    pos;
        std::string id;
        bool        upper;
        int         facing;
    };
    struct LastUse {
        std::int64_t tick;
        Key          pos;
    };

    void tick();
    void finishPlacement(Placed const& p);
    void handleGone(Gone const& g);

    std::vector<ll::event::ListenerPtr> mListeners;
    std::vector<Placed>                 mPlaced;
    std::vector<Used>                   mUsed;
    std::vector<Gone>                   mGone;
    std::map<Key, bool>                 mBroken;  ///< player breaks this tick: drop the item?
    std::map<std::int64_t, LastUse>     mLastUse; ///< player -> last use
    std::int64_t                        mNow = 0;
};

namespace {
struct Guard {
    Guard() { ++StoneBlocks::get().guard; }
    ~Guard() { --StoneBlocks::get().guard; }
    Guard(Guard const&)            = delete;
    Guard& operator=(Guard const&) = delete;
};

/// Sets both halves and runs the family's placement behaviour. `player` may be null (the placer
/// left before the next tick): the record is still made, no screen opens.
bool completeAt(
    BlockSource&     region,
    BlockPos const&  lower,
    StoneKind const& kind,
    int              facing,
    Player*          player,
    Placer const&    placer
) {
    if (!game::replaceable(region, above(lower))) return false;
    auto        s          = makeStone(kind, static_cast<int>(region.getDimensionId()), lower, facing);
    auto const* lowerBlock = game::blockWith(
        s.id,
        {
            {ids::kStateFacing, s.facing},
            {ids::kStateUpper,  0       }
    }
    );
    auto const* upperBlock = game::blockWith(
        s.id,
        {
            {ids::kStateFacing, s.facing},
            {ids::kStateUpper,  1       }
    }
    );
    if (!lowerBlock || !upperBlock) return false;
    {
        Guard g;
        // The placed lower half may already be in the wanted state (facing south is the default):
        // setting an identical block reports failure.
        if (&region.getBlock(lower) != lowerBlock && !game::setBlock(region, lower, *lowerBlock)) return false;
        if (!game::setBlock(region, above(lower), *upperBlock)) {
            game::clearBlock(region, lower);
            return false;
        }
    }
    if (kind.family == Family::Sharestone) onPlaced(player, placer, s);
    else portstone::onPlaced(placer, s);
    return true;
}
} // namespace

bool completePlacement(Player& player, BlockSource& region, BlockPos const& lower, StoneKind const& kind) {
    Placer placer{game::Waystones::playerKey(player), player.getRealName()};
    return completeAt(region, lower, kind, game::opposite(game::playerFacing(player)), &player, placer);
}

bool useStone(Player& player, BlockSource& region, BlockPos const& pos, bool sneaking) {
    auto s = stoneAt(region, pos);
    if (!s) return false;
    if (s->kind.family == Family::Sharestone) onUsed(player, *s, sneaking);
    else portstone::onUsed(player, *s);
    return true;
}

void StoneBlocks::finishPlacement(Placed const& p) {
    auto* region = regionOf(p.dim);
    if (!region) return;
    auto s = stoneAt(*region, p.pos);
    if (!s || s->y != p.pos.y) return;
    if (auto up = stoneAt(*region, above(p.pos)); up && up->y == p.pos.y) return; // already complete
    auto* player = playerById(p.player);
    if (completeAt(*region, p.pos, s->kind, p.facing, player, p.placer)) return;
    // No room for the upper half (Java refuses the placement): take the block back.
    if (stoneAt(*region, p.pos)) {
        Guard g;
        game::clearBlock(*region, p.pos);
    }
    if (p.refund) {
        auto item = game::makeItem(s->id);
        if (player) game::giveOrDrop(*player, item);
        else region->getLevel().getSpawner().spawnItem(*region, item, nullptr, p.pos.center(), 10);
    }
    log().info(
        "stone placement refused (no room above): {} at {} {} {} by {}",
        s->id,
        p.pos.x,
        p.pos.y,
        p.pos.z,
        p.placer.name
    );
}

void StoneBlocks::handleGone(Gone const& g) {
    auto* region = regionOf(g.dim);
    if (!region) return;
    auto kind = stoneKindOf(g.id);
    if (!kind) return;
    BlockPos lower = g.upper ? BlockPos{g.pos.x, g.pos.y - 1, g.pos.z} : g.pos;
    BlockPos other = g.upper ? lower : above(lower);
    // Neither half survives without the other.
    if (region->hasChunksAt(other, 0, false)) {
        auto const& b = region->getBlock(other);
        if (b.getTypeName() == g.id && (game::intState(b, ids::kStateUpper).value_or(0) != 0) != g.upper) {
            Guard guard;
            game::clearBlock(*region, other);
        }
    }
    if (kind->family == Family::Sharestone) onRemoved(makeStone(*kind, g.dim, lower, g.facing));
    // Survival pickaxe breaks drop one item, whichever half was mined.
    bool drop = false;
    for (auto const& pos : {lower, above(lower)}) {
        if (auto it = mBroken.find(keyOf(g.dim, pos)); it != mBroken.end()) {
            drop = drop || it->second;
            mBroken.erase(it);
        }
    }
    if (drop) {
        region->getLevel().getSpawner().spawnItem(*region, game::makeItem(g.id), nullptr, lower.center(), 10);
        ++gDrops;
        log().info("stone dropped: {} at {} {} {}", g.id, lower.x, lower.y, lower.z);
    }
}

void StoneBlocks::tick() {
    ++mNow;
    // Removals first: a stone broken and placed again at the same spot within one tick must not
    // lose its new half / record to the old removal.
    if (!mGone.empty()) {
        auto gone = std::move(mGone);
        mGone.clear();
        for (auto const& g : gone) handleGone(g);
    }
    if (!mPlaced.empty()) {
        auto placed = std::move(mPlaced);
        mPlaced.clear();
        for (auto const& p : placed) finishPlacement(p);
    }
    if (!mUsed.empty()) {
        auto used = std::move(mUsed);
        mUsed.clear();
        for (auto const& u : used) {
            auto* player = playerById(u.player);
            auto* region = regionOf(u.dim);
            if (player && region && player->isAlive()) useStone(*player, *region, u.pos, u.sneaking);
        }
    }
    mBroken.clear(); // break marks only matter for the tick they were made in
    if (mNow % 200 == 0)
        std::erase_if(mLastUse, [this](auto const& kv) { return mNow - kv.second.tick > kUseCooldownTicks; });
}

void StoneBlocks::install() {
    auto& bus = ll::event::EventBus::getInstance();
    mListeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([this](auto&) { tick(); }));

    // A player placed one of our blocks (the item places the lower half with default states).
    mListeners.push_back(
        bus.emplaceListener<ll::event::PlayerPlacedBlockEvent>([this](ll::event::PlayerPlacedBlockEvent& ev) {
            auto& player = ev.self();
            if (player.isClientSide()) return;
            auto const& block = ev.placedBlock();
            if (!stoneKindOf(block.getTypeName())) return;
            if (game::intState(block, ids::kStateUpper).value_or(0) != 0) return;
            mPlaced.push_back({
                player.getOrCreateUniqueID().rawID,
                Placer{game::Waystones::playerKey(player), player.getRealName()},
                !player.isCreative(),
                static_cast<int>(player.getDimensionId()),
                ev.pos(),
                game::opposite(game::playerFacing(player)),
            });
        })
    );

    mListeners.push_back(bus.emplaceListener<ll::event::PlayerInteractBlockEvent>(
        [this](ll::event::PlayerInteractBlockEvent& ev) {
            auto& player = ev.self();
            if (player.isClientSide() || ev.isCancelled()) return;
            auto&       region = player.getDimensionBlockSource();
            auto const& pos    = ev.blockPos();
            if (!stoneKindOf(game::blockIdAt(region, pos))) return;
            bool        sneaking = player.isSneaking();
            auto const& main     = player.getSelectedItem();
            // Java: sneaking with an item in either hand skips the block (the item is used / placed).
            if (sneaking && (!main.isNull() || !player.getOffhandSlot().isNull())) return;
            // A blank scroll binds the block through its own use-on.
            if (game::idOf(main) == ids::id(ids::kBlankScroll)) return;
            ev.cancel();
            auto key   = player.getOrCreateUniqueID().rawID;
            auto lower = game::lowerHalf(region, pos);
            auto where = keyOf(static_cast<int>(player.getDimensionId()), lower);
            if (auto it = mLastUse.find(key);
                it != mLastUse.end() && it->second.pos == where && mNow - it->second.tick < kUseCooldownTicks) {
                return; // the same click repeated while "use" is held
            }
            mLastUse[key] = {mNow, where};
            mUsed.push_back({key, static_cast<int>(player.getDimensionId()), pos, sneaking});
        },
        ll::event::EventPriority::High
    ));

    // Remember survival pickaxe breaks: the drop happens when the halves are cleaned up.
    mListeners.push_back(bus.emplaceListener<ll::event::PlayerDestroyBlockEvent>(
        [this](ll::event::PlayerDestroyBlockEvent& ev) {
            auto& player = ev.self();
            if (player.isClientSide() || ev.isCancelled()) return;
            auto& region = player.getDimensionBlockSource();
            if (!stoneKindOf(game::blockIdAt(region, ev.pos()))) return;
            mBroken[keyOf(static_cast<int>(player.getDimensionId()), ev.pos())] =
                breakDrops(player, player.getSelectedItem());
        },
        ll::event::EventPriority::Lowest
    ));

    mListeners.push_back(bus.emplaceListener<ll::event::BlockChangedEvent>([this](ll::event::BlockChangedEvent& ev) {
        if (guard > 0 || ev.layer() != 0) return;
        auto const& prev = ev.previousBlock();
        auto const& id   = prev.getTypeName();
        if (!ourNamespace(id) || ev.newBlock().getTypeName() == id) return;
        if (ev.blockSource().getLevel().isClientSide() || !stoneKindOf(id)) return;
        mGone.push_back({
            static_cast<int>(ev.blockSource().getDimensionId()),
            ev.pos(),
            id,
            game::intState(prev, ids::kStateUpper).value_or(0) != 0,
            game::intState(prev, ids::kStateFacing).value_or(0),
        });
    }));
}

void StoneBlocks::uninstall() {
    auto& bus = ll::event::EventBus::getInstance();
    for (auto& l : mListeners) bus.removeListener(l);
    mListeners.clear();
    mPlaced.clear();
    mUsed.clear();
    mGone.clear();
    mBroken.clear();
    mLastUse.clear();
}

} // namespace bs::feature::sharestone

BS_REGISTER_MODULE(bs::feature::sharestone::StoneBlocks);
