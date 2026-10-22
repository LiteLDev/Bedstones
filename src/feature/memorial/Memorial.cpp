// Epitaph + Fleeting Memorial (docs/SPEC.md §1.7).

#include "feature/memorial/Memorial.h"
#include "feature/portal/World.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/memory/Hook.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorDefinitionIdentifier.h"
#include "mc/world/actor/ActorEvent.h"
#include "mc/world/actor/Mob.h"
#include "mc/server/ServerPlayer.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/Spawner.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/phys/AABB.h"

#include "game/Blocks.h"
#include "game/Effects.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "service/Services.h"
#include "service/TargetProviders.h"

namespace bs::feature::memorial {

namespace {

using core::WarpSourceKind;
using portal::regionFor;

constexpr float kLabelHeight = 0.8f; ///< label entity y above the block's bottom

std::string memorialId() { return ids::id(ids::kFleetingMemorial); }

core::WaystoneRecord* memorialRecord(std::string const& uid) {
    auto* r = game::Waystones::get().find(uid);
    return r && r->type == core::types::kFleetingMemorial ? r : nullptr;
}

AABB cellBox(core::GlobalPos const& p) {
    return AABB{
        Vec3{static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z)},
        Vec3{static_cast<float>(p.x + 1), static_cast<float>(p.y + 2), static_cast<float>(p.z + 1)}
    };
}

std::vector<Actor*> labelsAt(BlockSource& region, core::GlobalPos const& p) {
    std::vector<Actor*> out;
    for (Actor* a : portal::entitiesIn(region, cellBox(p))) {
        if (a && a->getTypeName() == kLabelEntity) out.push_back(a);
    }
    return out;
}

void spawnLabel(BlockSource& region, core::WaystoneRecord const& r) {
    auto level = ll::service::getLevel();
    if (!level) return;
    Vec3 at{static_cast<float>(r.pos.x) + 0.5f, static_cast<float>(r.pos.y) + kLabelHeight, static_cast<float>(r.pos.z) + 0.5f};
    auto* mob = level->getSpawner().spawnMob(region, ActorDefinitionIdentifier(std::string(kLabelEntity)), nullptr, at, false, false, false);
    if (!mob) {
        log().warn("fleeting memorial: could not spawn the name label of {}", r.uid);
        return;
    }
    mob->setNameTag("§5" + r.ownerName); // purple, like the name engraved by Waystones
}

void removeLabels(BlockSource& region, core::GlobalPos const& p) {
    for (Actor* a : labelsAt(region, p)) a->despawn();
}

/// Landing spot for a warp to a memorial: in front of it, or on the memorial itself (it is
/// removed right after the warp).
service::Destination landingFor(core::WaystoneRecord const& r) {
    service::Destination d;
    d.block       = r.pos;
    d.targetUid   = r.uid;
    d.yaw         = static_cast<float>(r.facing) * 90.f;
    auto [dx, dz] = game::facingOffset(r.facing);
    int  x = r.pos.x, z = r.pos.z;
    if (auto* region = regionFor(r.pos.dim)) {
        BlockPos front{r.pos.x + dx, r.pos.y, r.pos.z + dz};
        if (game::replaceable(*region, front) && game::replaceable(*region, BlockPos{front.x, front.y + 1, front.z})) {
            x = front.x;
            z = front.z;
        }
    }
    d.x = static_cast<float>(x) + 0.5f;
    d.y = static_cast<float>(r.pos.y);
    d.z = static_cast<float>(z) + 0.5f;
    return d;
}

/// Source kinds whose menus list memorials (docs/SPEC.md §3.7).
bool listsMemorials(WarpSourceKind k) {
    switch (k) {
    case WarpSourceKind::Waystone:
    case WarpSourceKind::Sharestone:
    case WarpSourceKind::WarpScroll:
    case WarpSourceKind::WarpStone:
    case WarpSourceKind::PortalScroll:
    case WarpSourceKind::InventoryButton:
        return true;
    default:
        return false;
    }
}

void tick(Level& level) {
    static std::uint64_t ticks = 0;
    ++ticks;
    bool maintain = (ticks % 20) == 0; // block / label checks once a second
    bool ash      = (ticks % 10) == 0;
    if (!maintain && !ash) return;
    auto& ws = game::Waystones::get();
    if (!ws.available()) return;
    std::vector<std::string> uids;
    for (auto* r : ws.ofType(core::types::kFleetingMemorial)) uids.push_back(r->uid);
    for (auto const& uid : uids) {
        auto* r = memorialRecord(uid);
        if (!r) continue;
        auto* region = regionFor(r->pos.dim);
        if (!region) continue;
        auto pos = game::blockPos(r->pos);
        if (!region->hasChunksAt(pos, 0, false)) continue;
        // Only memorials someone can see get particles and labels.
        bool watched = false;
        Vec3 c{static_cast<float>(r->pos.x) + 0.5f, static_cast<float>(r->pos.y) + 0.5f, static_cast<float>(r->pos.z) + 0.5f};
        level.forEachPlayer([&](Player& p) {
            if (static_cast<int>(p.getDimensionId()) == r->pos.dim && p.getPosition().distanceTo(c) <= 48.f) watched = true;
            return !watched;
        });
        if (maintain) {
            if (game::blockIdAt(*region, pos) != memorialId()) {
                log().info("fleeting memorial removed: block of {} is gone", uid);
                removeMemorial(uid);
                continue;
            }
            if (watched && labelsAt(*region, r->pos).empty()) spawnLabel(*region, *r);
        }
        if (ash && watched) game::particle(r->pos.dim, "bedstones:memorial_ash", Vec3{c.x, c.y + 0.4f, c.z});
    }
    // Labels whose memorial is gone (e.g. removed while the chunk was unloaded).
    if (maintain) {
        level.forEachPlayer([&](Player& p) {
            auto& region = p.getDimensionBlockSource();
            auto  at     = p.getPosition();
            AABB  box{Vec3{at.x - 32.f, at.y - 32.f, at.z - 32.f}, Vec3{at.x + 32.f, at.y + 32.f, at.z + 32.f}};
            for (Actor* a : portal::entitiesIn(region, box)) {
                if (!a || a->getTypeName() != kLabelEntity) continue;
                auto        lp = a->getPosition();
                core::GlobalPos cell{
                    static_cast<int>(p.getDimensionId()),
                    static_cast<int>(std::floor(lp.x)),
                    static_cast<int>(std::floor(lp.y - kLabelHeight + 0.01f)),
                    static_cast<int>(std::floor(lp.z))
                };
                auto* r = ws.findAt(cell);
                if (!r || r->type != core::types::kFleetingMemorial) a->despawn();
            }
            return true;
        });
    }
}

// Runs before the death is processed, while the inventory is still intact (LeviLamina's
// PlayerDieEvent fires after the items were dropped).
LL_TYPE_INSTANCE_HOOK(
    MemorialDeathHook,
    ll::memory::HookPriority::Normal,
    ServerPlayer,
    &ServerPlayer::$die,
    void,
    ActorDamageSource const& source
) {
    try {
        onPlayerDeath(*this);
    } catch (std::exception const& e) {
        log().error("fleeting memorial: death handling failed: {}", e.what());
    }
    origin(source);
}

class MemorialModule {
public:
    void install() {
        auto& bus = ll::event::EventBus::getInstance();
        MemorialDeathHook::hook();
        mListeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([](ll::event::ServerLevelTickEvent& ev) {
            tick(ev.level());
        }));
    }
    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
        MemorialDeathHook::unhook();
    }

private:
    std::vector<ll::event::ListenerPtr> mListeners;
};

} // namespace

std::string onPlayerDeath(Player& player) {
    auto epitaph = game::findItem(player, [](ItemStack const& s) { return game::idOf(s) == ids::id(ids::kEpitaph); });
    if (!epitaph) return {};
    auto& region = player.getDimensionBlockSource();
    auto  feet   = player.getFeetBlockPos();
    auto  spot   = core::portal::findMemorialSpot({feet.x, feet.y, feet.z}, [&](int x, int y, int z) {
        return game::replaceable(region, BlockPos{x, y, z});
    });
    if (!spot) {
        log().info("fleeting memorial: no space near {}", player.getRealName());
        return {};
    }
    int  dim    = static_cast<int>(player.getDimensionId());
    int  facing = game::opposite(game::playerFacing(player));
    auto uid    = spawnMemorial(player, dim, *spot, facing);
    if (uid.empty()) return {};
    game::consumeAt(player, *epitaph, 1, false);
    // Totem animation + sound (the Bedrock client draws its own totem model).
    if (auto level = ll::service::getLevel()) level->broadcastActorEvent(player, ActorEvent::TalismanActivate, 0, std::nullopt);
    game::soundAround(dim, "random.totem", player.getPosition(), 1.0f, 1.0f);
    log().info("fleeting memorial placed: {} for {} at {} {} {} facing {}", uid, player.getRealName(), spot->x, spot->y, spot->z, facing);
    return uid;
}

std::string spawnMemorial(Player const& owner, int dim, core::portal::Cell cell, int facing) {
    auto& ws     = game::Waystones::get();
    auto* region = regionFor(dim);
    if (!region || !ws.available()) return {};
    BlockPos pos{cell.x, cell.y, cell.z};
    if (!game::replaceable(*region, pos)) return {};
    auto const* block = game::blockWith(memorialId(), {{ids::kStateFacing, facing & 3}});
    if (!block || !game::setBlock(*region, pos, *block)) return {};
    core::WaystoneRecord rec;
    rec.type       = std::string(core::types::kFleetingMemorial);
    rec.name       = tr("block.waystones.fleeting_memorial", "Fleeting Memorial");
    rec.pos        = {dim, cell.x, cell.y, cell.z};
    rec.style      = std::string(ids::kFleetingMemorial);
    rec.facing     = facing & 3;
    rec.origin     = core::Origin::Player;
    rec.ownerUid   = game::Waystones::playerKey(owner);
    rec.ownerName  = owner.getRealName();
    rec.visibility = core::Visibility::Activation;
    auto* r        = ws.create(std::move(rec));
    if (!r) {
        game::clearBlock(*region, pos);
        return {};
    }
    spawnLabel(*region, *r);
    return r->uid;
}

void removeMemorial(std::string const& uid) {
    auto* r = memorialRecord(uid);
    if (!r) return;
    auto p = r->pos;
    if (auto* region = regionFor(p.dim)) {
        auto pos = game::blockPos(p);
        if (region->hasChunksAt(pos, 0, false)) {
            if (game::blockIdAt(*region, pos) == memorialId()) game::clearBlock(*region, pos);
            removeLabels(*region, p);
        }
    }
    game::Waystones::get().remove(uid);
}

int labelCount(std::string const& uid) {
    auto* r = memorialRecord(uid);
    if (!r) return 0;
    auto* region = regionFor(r->pos.dim);
    if (!region) return 0;
    int n = 0;
    for (Actor* a : labelsAt(*region, r->pos)) n += a->isAlive() ? 1 : 0;
    return n;
}

} // namespace bs::feature::memorial

BS_REGISTER_MODULE(bs::feature::memorial::MemorialModule);

// The player's own memorials, pinned first (docs/SPEC.md §3.7). Warping to one destroys it.
BS_REGISTER_TARGET_PROVIDER(bs::service::TargetProvider{
    .name = "fleeting_memorial",
    .list =
        [](Player& player, bs::core::WarpSource const& source, std::vector<bs::core::WarpTarget>& out) {
            using namespace bs;
            if (!feature::memorial::listsMemorials(source.kind)) return;
            auto  key  = game::Waystones::playerKey(player);
            auto& ws   = game::Waystones::get();
            auto& data = ws.playerData(player);
            auto& reqs = service::get<service::IRequirements>();
            for (auto* r : ws.ofType(core::types::kFleetingMemorial)) {
                if (r->ownerUid != key) continue;
                if (std::any_of(out.begin(), out.end(), [&](auto const& t) { return t.id == r->uid; })) continue;
                core::WarpTarget t;
                t.id          = r->uid;
                t.kind        = std::string(core::types::kFleetingMemorial);
                t.name        = data.effectiveName(r->uid, r->name);
                t.pos         = r->pos;
                t.pinned      = true;
                t.requirement = reqs.resolve(player, source, t);
                t.canAfford   = reqs.canAfford(player, t.requirement);
                out.push_back(std::move(t));
            }
        },
    .resolve = [](Player& player, std::string const& id) -> std::optional<bs::service::Destination> {
        using namespace bs;
        auto* r = feature::memorial::memorialRecord(id);
        if (!r || r->ownerUid != game::Waystones::playerKey(player)) return std::nullopt;
        return feature::memorial::landingFor(*r);
    },
    .owns = [](std::string const& id) { return bs::feature::memorial::memorialRecord(id) != nullptr; },
    .onWarped =
        [](Player& player, std::string const& id) {
            bs::log().info("fleeting memorial used: {} by {}", id, player.getRealName());
            bs::feature::memorial::removeMemorial(id);
        },
});
