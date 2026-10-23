#include "feature/scrolls/Scrolls.h"

#include <cmath>
#include <numbers>
#include <random>
#include <unordered_map>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/entity/ActorHurtEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "ll/api/event/player/PlayerUseItemEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"

#include "game/Blocks.h"
#include "game/Effects.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "service/Services.h"
#include "service/WarpEvents.h"

namespace bs::feature::scrolls {

using core::scrolls::ScrollItem;

namespace {

struct Charge {
    ScrollItem  item;
    std::string itemId;
    int         hand     = 0;
    int         slot     = 0; ///< hotbar slot when it started (main hand)
    int         elapsed  = 0;
    int         duration = 0;
};

std::unordered_map<std::int64_t, Charge>& charges() {
    static std::unordered_map<std::int64_t, Charge> m;
    return m;
}

/// Blank scroll uses waiting for the next tick (a cancelled interaction rolls back this tick's edits).
struct PendingBind {
    std::int64_t player;
    std::string  uid;
};
std::vector<PendingBind>& pendingBinds() {
    static std::vector<PendingBind> v;
    return v;
}
std::unordered_map<std::int64_t, std::uint64_t>& lastBindTick() {
    static std::unordered_map<std::int64_t, std::uint64_t> m;
    return m;
}
std::uint64_t gTick = 0;

std::mt19937& rng() {
    static thread_local std::mt19937 r{std::random_device{}()};
    return r;
}

std::int64_t keyOf(Player const& p) { return p.getOrCreateUniqueID().rawID; }

Player* playerByKey(std::int64_t key) {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    ActorUniqueID id;
    id.rawID = key;
    return level->getPlayer(id);
}

std::optional<ScrollItem> scrollOf(ItemStack const& stack) { return core::scrolls::fromPath(ids::pathOf(game::idOf(stack))); }

int dimOf(Player const& p) { return static_cast<int>(p.getDimensionId()); }

/// Whether the game still has the player in its item-use state (bow pose, held right-click).
bool usingItem(Player const& p) { return !p.mItemInUse->mItem->isNull(); }

/// Ends the server-side item use (Java's `finishUsingItem` / `stopUsingItem`).
void finishUse(Player& p) { p.stopUsingItem(); }

// ---- particles ------------------------------------------------------------------------------------------

/// Server-side particle packets are far more expensive than Java's client-side particles: spawn a
/// third of Waystones' counts (same schedule and shape).
constexpr int kParticleDivisor = 3;

void spawnChargeParticles(Player& p, Charge const& c) {
    auto counts = core::scrolls::chargeParticles(c.item, c.elapsed, c.duration);
    if (!counts.any()) return;
    auto&                                     r = rng();
    std::uniform_real_distribution<float>     dist(0.f, 1.f);
    auto                                      u = [&] { return dist(r); };
    int                                       dim  = dimOf(p);
    Vec3                                      feet = p.getFeetPos();
    // Around the body, in a ring 0.6-0.9 blocks out and below the eyes: a particle right at the
    // camera fills the whole first-person view (Java's are client-side and tiny there).
    auto around = [&](std::string const& name, int count, float yOffset) {
        count = (count + kParticleDivisor - 1) / kParticleDivisor;
        for (int i = 0; i < count; ++i) {
            float angle = u() * 2.f * std::numbers::pi_v<float>;
            float dist  = 0.6f + 0.3f * u();
            float y     = std::min(yOffset + u(), 1.2f);
            game::particle(dim, name, Vec3{feet.x + std::cos(angle) * dist, feet.y + y, feet.z + std::sin(angle) * dist});
        }
    };
    if (c.item == ScrollItem::WarpStone) {
        // A few reverse-portal particles anchored at the hand holding the stone.
        float yaw = p.getRotation().y * std::numbers::pi_v<float> / 180.f;
        float ox = c.hand == 0 ? -0.30f : 0.30f, oz = 0.52f;
        float a  = -yaw;
        float hx = ox * std::cos(a) + oz * std::sin(a), hz = oz * std::cos(a) - ox * std::sin(a);
        int   handCount = std::min(4, counts.reversePortal);
        for (int i = 0; i < handCount; ++i) {
            game::particle(dim, "minecraft:portal_reverse_particle",
                Vec3{feet.x + hx + (u() - 0.5f) * 0.5f, feet.y + 0.8f + 0.4f * u(), feet.z + hz + (u() - 0.5f) * 0.5f});
        }
        around("minecraft:portal_reverse_particle", counts.reversePortal - handCount, 0.f);
    } else {
        around("minecraft:portal_reverse_particle", counts.reversePortal, 0.f);
    }
    around("minecraft:crimson_spore_particle", counts.crimsonSpore, 0.f);
    // Java's WITCH sparks: Bedrock's witchspell emitter spawns large rising puffs that cover the
    // first-person view, the small portal sparks look closer.
    around("minecraft:portal_directional", counts.witch, 0.5f);
    around(c.item == ScrollItem::WarpStone ? "minecraft:portal_reverse_particle" : "minecraft:portal_directional", counts.portal, 1.f);
}

// ---- consumption ------------------------------------------------------------------------------------------

/// Where the warp item of `request` is now: the hand it was used from, else the first matching
/// stack (bound scrolls: bound to the warp's target).
std::optional<game::ItemLocation> locateWarpItem(Player& p, service::WarpRequest const& r) {
    bool bound   = r.source.kind == core::WarpSourceKind::BoundScroll;
    auto matches = [&](ItemStack const& s) {
        if (game::idOf(s) != r.source.itemId) return false;
        return !bound || game::userString(s, ids::kNbtBoundUid) == r.targetId;
    };
    auto loc = game::handSlot(p, r.source.hand);
    if (matches(game::itemAt(p, loc))) return loc;
    return game::findItem(p, matches);
}

void onWarped(Player& p, service::WarpRequest const& r) {
    auto item = core::scrolls::fromPath(ids::pathOf(r.source.itemId));
    if (!item || !core::scrolls::sourceKind(*item)) return;
    if (p.isCreative()) return; // creative players keep their scrolls and warp stones
    auto loc = locateWarpItem(p, r);
    if (!loc) {
        log().warn("warp item {} of {} not found after the warp", r.source.itemId, p.getRealName());
        return;
    }
    if (core::scrolls::consumedOnWarp(*item)) {
        game::consumeAt(p, *loc, 1, true);
        auto const& left = game::itemAt(p, *loc);
        log().info("scroll consumed: {} by {} (left {})", r.source.itemId, p.getRealName(), game::idOf(left) == r.source.itemId ? left.mCount : 0);
        return;
    }
    if (*item != ScrollItem::WarpStone || !config::get<ItemsConfig>().enableDurability) return;
    ItemStack   copy = game::itemAt(p, *loc);
    Item const* type = copy.mItem.get();
    int         max  = type ? type->getMaxDamage() : 0;
    copy.hurtAndBreak(1, &p); // honours Unbreaking
    // Java breaks an item once its damage reaches the maximum (128 warps).
    bool broken = copy.isNull() || copy.mCount == 0 || (max > 0 && copy.getDamageValue() >= max);
    if (broken) {
        game::setItemAt(p, *loc, ItemStack::EMPTY_ITEM());
        std::uniform_real_distribution<float> pitch(0.8f, 1.2f);
        game::soundAround(dimOf(p), "random.break", p.getPosition(), 0.8f, pitch(rng()));
        log().info("warp stone broke: {}", p.getRealName());
    } else {
        game::setItemAt(p, *loc, copy);
        log().info("warp stone damaged: {}/{} by {}", copy.getDamageValue(), max, p.getRealName());
    }
}

BS_REGISTER_WARP_LISTENER([](Player& p, service::WarpRequest const& r, service::Destination const&) { onWarped(p, r); });

// ---- binding ------------------------------------------------------------------------------------------------

/// Whether `stack` fits into the inventory (an empty slot or a stack it merges with).
bool inventoryHasRoomFor(Player& p, ItemStack const& stack) {
    auto& inv = p.getInventory();
    int   n   = std::min(inv.getContainerSize(), 36);
    for (int i = 0; i < n; ++i) {
        auto const& s = inv.getItem(i);
        if (s.isNull()) return true;
        if (s.isStackable(stack) && s.mCount < s.getMaxStackSize()) return true;
    }
    return false;
}

// ---- module ---------------------------------------------------------------------------------------------------

void tick() {
    ++gTick;
    auto& binds = pendingBinds();
    if (!binds.empty()) {
        auto todo = std::move(binds);
        binds.clear();
        for (auto const& b : todo) {
            if (auto* p = playerByKey(b.player)) bindBlankScroll(*p, b.uid, 0);
        }
    }
    if (gTick % 200 == 0) std::erase_if(lastBindTick(), [](auto const& e) { return gTick - e.second > 20; });
    auto& m = charges();
    if (m.empty()) return;
    std::vector<std::pair<std::int64_t, Charge>> done;
    std::vector<std::pair<std::int64_t, std::string>> cancelled;
    for (auto& [key, c] : m) {
        auto* p = playerByKey(key);
        if (!p) {
            cancelled.emplace_back(key, "");
            continue;
        }
        auto loc  = game::handSlot(*p, c.hand);
        bool held = game::idOf(game::itemAt(*p, loc)) == c.itemId && (c.hand == 1 || loc.slot == c.slot);
        // The game enters the item-use state right after the use event: give it one tick.
        bool released = c.elapsed >= 1 && !usingItem(*p);
        if (!held || released) {
            cancelled.emplace_back(key, held ? "released" : "item changed");
            continue;
        }
        ++c.elapsed;
        spawnChargeParticles(*p, c);
        if (c.elapsed >= c.duration) done.emplace_back(key, c);
    }
    for (auto const& [key, reason] : cancelled) {
        auto* p = playerByKey(key);
        if (p) cancelCharge(*p, reason);
        else m.erase(key);
    }
    for (auto const& [key, c] : done) {
        m.erase(key);
        auto* p = playerByKey(key);
        if (!p) continue;
        finishUse(*p);
        completeCharge(*p, c.item, c.hand);
    }
}


class ScrollsModule {
public:
    void install() {
        auto& bus = ll::event::EventBus::getInstance();
        mListeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([](auto&) { tick(); }));
        mListeners.push_back(bus.emplaceListener<ll::event::PlayerUseItemEvent>([](ll::event::PlayerUseItemEvent& ev) {
            auto& p = ev.self();
            if (p.isClientSide()) return;
            auto item = scrollOf(ev.item());
            if (!item || !core::scrolls::chargeable(*item)) return;
            if (isCharging(p)) return; // Bedrock may repeat the use while the button is held
            startCharge(p, 0);
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::PlayerInteractBlockEvent>(
            [](ll::event::PlayerInteractBlockEvent& ev) {
                auto& p = ev.self();
                if (p.isClientSide() || scrollOf(ev.item()) != ScrollItem::BlankScroll) return;
                auto* rec = recordAtBlock(p, game::globalPos(dimOf(p), ev.blockPos()));
                if (!rec) return;
                ev.cancel(); // the scroll is used on the block, not the block itself
                auto key  = keyOf(p);
                auto& last = lastBindTick()[key];
                if (last != 0 && gTick - last < 4) return; // one bind per click (the client may send several)
                last = gTick;
                pendingBinds().push_back({key, rec->uid});
            },
            ll::event::EventPriority::High
        ));
        mListeners.push_back(bus.emplaceListener<ll::event::ActorHurtEvent>([](ll::event::ActorHurtEvent& ev) {
            if (ev.isCancelled() || ev.damage() <= 0 || !ev.self().isPlayer()) return;
            auto& p = static_cast<Player&>(ev.self());
            if (!isCharging(p)) return;
            cancelCharge(p, "damaged"); // Waystones' IResetUseOnDamage
            finishUse(p);
        }));
    }
    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
        charges().clear();
        pendingBinds().clear();
        lastBindTick().clear();
    }

private:
    std::vector<ll::event::ListenerPtr> mListeners;
};

} // namespace

BS_REGISTER_CONFIG("items", ItemsConfig);
BS_REGISTER_MODULE(ScrollsModule);

// ---- public API ----------------------------------------------------------------------------------------------

bool startCharge(Player& p, int hand) {
    auto        loc   = game::handSlot(p, hand);
    auto const& stack = game::itemAt(p, loc);
    auto        item  = scrollOf(stack);
    if (!item || !core::scrolls::chargeable(*item) || isCharging(p)) return false;
    auto const& cfg      = config::get<ItemsConfig>();
    int         duration = core::scrolls::useTime(*item, cfg.warpStoneUseTime, cfg.scrollUseTime);
    // Waystones plays PORTAL_TRIGGER when the use starts.
    game::soundAround(dimOf(p), "portal.trigger", p.getPosition(), 0.1f, 2.f);
    if (duration <= 0) {
        completeCharge(p, *item, hand);
        return true;
    }
    Charge c{*item, game::idOf(stack), hand, loc.slot, 0, duration};
    charges()[keyOf(p)] = c;
    // Players without a client-side use (simulated players) still need the use state the charge
    // checks; real clients enter it themselves right after this event.
    if (!usingItem(p)) p.startUsingItem(stack, 72000);
    log().info("scroll charging: {} by {} ({} ticks)", c.itemId, p.getRealName(), duration);
    return true;
}

bool isCharging(Player const& p) { return charges().contains(keyOf(p)); }

void cancelCharge(Player& p, std::string const& reason) {
    auto it = charges().find(keyOf(p));
    if (it == charges().end()) return;
    log().info("scroll charge cancelled: {} by {} ({})", it->second.itemId, p.getRealName(), reason);
    charges().erase(it);
}

std::string returnTargetFor(Player& p) {
    auto& ws   = game::Waystones::get();
    auto& data = ws.playerData(p);
    std::string portal;
    bool        portalExists = false;
    if (auto it = data.extra.find("returnPortal"); it != data.extra.end() && it->is_string()) {
        portal = it->get<std::string>();
        auto* r = portal.empty() ? nullptr : ws.find(portal);
        portalExists = r && r->type == core::types::kWarpPortal;
    }
    std::vector<core::scrolls::Candidate> candidates;
    for (auto const& uid : data.activated) {
        if (auto* r = ws.find(uid); r && r->isWaystone()) candidates.push_back({r->uid, r->pos});
    }
    Vec3 feet    = p.getFeetPos();
    auto nearest = core::scrolls::nearest(candidates, dimOf(p), feet.x, feet.y, feet.z);
    return core::scrolls::returnTarget(portal, portalExists, nearest);
}

void completeCharge(Player& p, ScrollItem item, int hand) {
    auto kind = core::scrolls::sourceKind(item);
    if (!kind) return;
    auto        loc   = game::handSlot(p, hand);
    auto const& stack = game::itemAt(p, loc);
    if (scrollOf(stack) != item) return; // Waystones: the warp item must still be held
    core::WarpSource source;
    source.kind   = *kind;
    source.itemId = game::idOf(stack);
    source.hand   = hand;
    switch (item) {
    case ScrollItem::WarpScroll:
    case ScrollItem::WarpStone:
        log().info("scroll charged: {} by {}, opening selection", source.itemId, p.getRealName());
        service::get<service::ISelection>().open(p, source);
        return;
    case ScrollItem::BoundScroll: {
        auto uid = game::userString(stack, ids::kNbtBoundUid);
        if (!uid) {
            game::actionBar(p, "§4" + trFor(p, "chat.waystones.scroll_not_yet_bound", "This scroll is not bound to a waystone."));
            return;
        }
        log().info("scroll charged: {} by {}, warping to {}", source.itemId, p.getRealName(), *uid);
        service::get<service::ITeleport>().warp(p, service::WarpRequest{source, *uid});
        return;
    }
    case ScrollItem::ReturnScroll: {
        auto target = returnTargetFor(p);
        if (target.empty()) {
            // Waystones does nothing when there is no waystone to return to.
            log().info("scroll charged: {} by {}, no return target", source.itemId, p.getRealName());
            return;
        }
        log().info("scroll charged: {} by {}, warping to {}", source.itemId, p.getRealName(), target);
        service::get<service::ITeleport>().warp(p, service::WarpRequest{source, target});
        return;
    }
    case ScrollItem::BlankScroll:
        return;
    }
}

core::WaystoneRecord* recordAtBlock(Player& p, core::GlobalPos const& pos) {
    auto& ws = game::Waystones::get();
    if (auto* r = ws.findAt(pos)) return r;
    if (pos.dim != dimOf(p)) return nullptr;
    auto& region = p.getDimensionBlockSource();
    auto  lower  = game::lowerHalf(region, game::blockPos(pos));
    if (lower.y == pos.y) return nullptr;
    return ws.findAt(game::globalPos(pos.dim, lower));
}

bool bindBlankScroll(Player& p, std::string const& uid, int hand) {
    auto* rec = game::Waystones::get().find(uid);
    if (!rec) return false;
    auto        loc   = game::handSlot(p, hand);
    auto const& blank = game::itemAt(p, loc);
    if (scrollOf(blank) != ScrollItem::BlankScroll) return false;
    ItemStack bound = game::makeItem(ids::id(ids::kBoundScroll));
    if (bound.isNull()) return false;
    game::setUserString(bound, ids::kNbtBoundUid, rec->uid);
    game::setUserString(bound, ids::kNbtBoundName, rec->name);
    game::setDisplay(bound, "", {core::scrolls::boundLore(trFor(p, "tooltip.waystones.bound_to", "Bound to: {}"), rec->name)});
    if (!core::scrolls::canBind(inventoryHasRoomFor(p, bound), p.isCreative(), blank.mCount)) {
        log().info("scroll bind failed: no room for {}", p.getRealName());
        return false;
    }
    game::consumeAt(p, loc, 1, true);
    game::giveOrDrop(p, bound);
    log().info("scroll bound: {} to {} by {}", rec->name, rec->uid, p.getRealName());
    return true;
}

} // namespace bs::feature::scrolls
