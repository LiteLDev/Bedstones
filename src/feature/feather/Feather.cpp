// Twinbound Feather: linking ritual, item data and the "player:<uuid>" target provider.

#include "feature/feather/Feather.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/platform/UUID.h"
#include "mc/world/ContainerID.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/PlayerItemInUse.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/Level.h"

#include "core/Warp.h"
#include "game/Effects.h"
#include "game/Ids.h"
#include "game/Waystones.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "service/Services.h"
#include "service/TargetProviders.h"

namespace bs::feature::feather {

namespace cf = core::feather;

namespace {

/// User-data key of the partner names shown in the lore (comma separated, unit-owned).
constexpr std::string_view kNbtFeatherNames = "bs_feather_names";
constexpr std::string_view kTargetPrefix    = "player:";

cf::TwinningTracker   gTracker;
std::set<std::string> gSimulatedUse; // player uuids holding use without a client
std::map<std::string, std::uint64_t> gUseStart; // tick each player started holding use
std::uint64_t                        gTick = 0;

/// Java's use duration (72000 ticks): the use is stopped before the food-backed item would finish.
constexpr std::uint64_t kMaxUseTicks = 72000;

std::string uuidOf(Player const& p) { return game::Waystones::playerKey(p); }

cf::Vec toVec(Vec3 const& v) { return {v.x, v.y, v.z}; }

/// The feather being used: the hand the use started from, else the main hand, else the offhand.
std::optional<game::ItemLocation> handFeather(Player& p) {
    bool offhandFirst = p.mItemInUse->mSlot->mContainerId == ContainerID::Offhand;
    for (int hand : offhandFirst ? std::initializer_list<int>{1, 0} : std::initializer_list<int>{0, 1}) {
        auto loc = game::handSlot(p, hand);
        if (isFeather(game::itemAt(p, loc))) return loc;
    }
    return std::nullopt;
}

/// The feather to link: the one in hand, else the first one carried.
std::optional<game::ItemLocation> linkFeather(Player& p) {
    if (auto loc = handFeather(p)) return loc;
    return game::findItem(p, [](ItemStack const& s) { return isFeather(s); });
}

std::optional<game::ItemLocation> freeSlot(Player& p) {
    int size = p.getInventory().getContainerSize();
    for (int i = 0; i < size; ++i) {
        game::ItemLocation loc{game::ItemLocation::Kind::Inventory, i};
        if (game::itemAt(p, loc).isNull()) return loc;
    }
    return std::nullopt;
}

void refreshLore(ItemStack& stack, Player& holder) {
    auto names = cf::parseLinks(game::userString(stack, kNbtFeatherNames).value_or(""));
    std::vector<std::string> lore;
    for (auto const& n : names) lore.push_back("§r§7" + trFor(holder, "bs.feather.tooltip.linked_to", "Linked to: {}", "§d" + n + "§7"));
    if (!lore.empty()) game::setDisplay(stack, "", lore);
}

void rememberLink(Player& holder, Player& partner, std::string const& ownId, std::string const& partnerId) {
    auto& ws   = game::Waystones::get();
    auto  key  = uuidOf(holder);
    auto& data = ws.playerData(key);
    auto& f    = data.extra["feather"];
    if (!f.is_object()) f = nlohmann::json::object();
    if (!f["links"].is_object()) f["links"] = nlohmann::json::object();
    f["links"][uuidOf(partner)] = {
        {"name",           partner.getRealName()},
        {"feather",        ownId                },
        {"partnerFeather", partnerId            },
    };
    ws.markPlayerDirty(key);
}

/// Links the feathers at `la` / `lb` both ways. False when they already were linked.
bool linkAt(Player& a, game::ItemLocation la, Player& b, game::ItemLocation lb) {
    ItemStack fa = game::itemAt(a, la);
    ItemStack fb = game::itemAt(b, lb);
    if (!isFeather(fa) || !isFeather(fb)) return false;
    if (cf::areLinked(infoOf(fa), infoOf(fb))) return false;
    auto ensureId = [](ItemStack& s) {
        auto id = game::userString(s, ids::kNbtFeather);
        if (id) return *id;
        auto fresh = core::randomUuid();
        game::setUserString(s, ids::kNbtFeather, fresh);
        return fresh;
    };
    auto ia = ensureId(fa);
    auto ib = ensureId(fb);
    game::setUserString(fa, ids::kNbtFeatherLink, cf::withLink(game::userString(fa, ids::kNbtFeatherLink).value_or(""), ib));
    game::setUserString(fb, ids::kNbtFeatherLink, cf::withLink(game::userString(fb, ids::kNbtFeatherLink).value_or(""), ia));
    game::setUserString(fa, kNbtFeatherNames, cf::withLink(game::userString(fa, kNbtFeatherNames).value_or(""), b.getRealName()));
    game::setUserString(fb, kNbtFeatherNames, cf::withLink(game::userString(fb, kNbtFeatherNames).value_or(""), a.getRealName()));
    refreshLore(fa, a);
    refreshLore(fb, b);
    game::setItemAt(a, la, fa);
    game::setItemAt(b, lb, fb);
    rememberLink(a, b, ia, ib);
    rememberLink(b, a, ib, ia);
    return true;
}

void stopUsing(Player& p) {
    gSimulatedUse.erase(uuidOf(p));
    p.stopUsingItem();
}

Player* onlinePlayer(std::string const& uuid) {
    auto level = ll::service::getLevel();
    if (!level || uuid.empty()) return nullptr;
    auto* p = level->getPlayer(mce::UUID::fromString(uuid));
    return p && !p->isClientSide() ? p : nullptr;
}

/// Menus that list Twinbound partners (docs/SPEC.md §3.7).
bool listsPartners(core::WarpSourceKind kind) {
    using K = core::WarpSourceKind;
    return kind == K::Waystone || kind == K::WarpScroll || kind == K::WarpStone || kind == K::InventoryButton;
}

} // namespace

bool isFeather(ItemStack const& stack) { return game::idOf(stack) == ids::id(ids::kTwinboundFeather); }

cf::FeatherInfo infoOf(ItemStack const& stack) {
    return {game::userString(stack, ids::kNbtFeather).value_or(""), game::userString(stack, ids::kNbtFeatherLink).value_or("")};
}

std::vector<cf::FeatherInfo> carriedFeathers(Player& player) {
    std::vector<cf::FeatherInfo> out;
    int                          size = player.getInventory().getContainerSize();
    for (int i = 0; i < size; ++i) {
        auto const& s = game::itemAt(player, {game::ItemLocation::Kind::Inventory, i});
        if (isFeather(s)) out.push_back(infoOf(s));
    }
    if (auto const& off = game::itemAt(player, {game::ItemLocation::Kind::Offhand, 0}); isFeather(off)) out.push_back(infoOf(off));
    return out;
}

bool arePartners(Player& a, Player& b) {
    if (&a == &b || a.isClientSide() || b.isClientSide()) return false;
    if (uuidOf(a) == uuidOf(b)) return false;
    return cf::anyLinked(carriedFeathers(a), carriedFeathers(b));
}

std::vector<Player*> partnersOf(Player& player) {
    std::vector<Player*> out;
    auto                 mine = carriedFeathers(player);
    bool any = std::any_of(mine.begin(), mine.end(), [](auto const& f) { return !f.id.empty() && !f.links.empty(); });
    if (!any) return out;
    auto level = ll::service::getLevel();
    if (!level) return out;
    auto self = uuidOf(player);
    level->forEachPlayer([&](Player& other) {
        if (!other.isClientSide() && uuidOf(other) != self && cf::anyLinked(mine, carriedFeathers(other))) out.push_back(&other);
        return true;
    });
    return out;
}

bool isUsingFeather(Player& player) {
    if (player.isClientSide()) return false;
    if (auto const& inUse = player.mItemInUse->mItem.get(); !inUse.isNull() && isFeather(inUse)) return true;
    auto it = gSimulatedUse.find(uuidOf(player));
    if (it == gSimulatedUse.end()) return false;
    if (handFeather(player)) return true;
    gSimulatedUse.erase(it);
    return false;
}

void simulateUse(Player& player, bool using_) {
    if (using_) gSimulatedUse.insert(uuidOf(player));
    else gSimulatedUse.erase(uuidOf(player));
}

int tickRitual() {
    auto level = ll::service::getLevel();
    if (!level) return 0;
    ++gTick;
    // Every online player can be hit by a look ray (Java picks any player); only feather users twin.
    struct Entry {
        Player*     p;
        std::string uuid;
        cf::Vec     feet;
        int         dim;
        bool        using_;
    };
    std::vector<Entry> players;
    level->forEachPlayer([&](Player& p) {
        if (!p.isClientSide() && p.isAlive()) {
            players.push_back({&p, uuidOf(p), toVec(p.getFeetPos()), static_cast<int>(p.getDimensionId()), isUsingFeather(p)});
        }
        return true;
    });
    std::vector<std::string> active;
    for (auto& e : players) {
        if (!e.using_) continue;
        // Stop a vanilla use at Java's use duration, before the food-backed item would be eaten.
        auto start = gUseStart.try_emplace(e.uuid, gTick).first->second;
        if (gTick - start >= kMaxUseTicks) {
            stopUsing(*e.p);
            e.using_ = false;
            continue;
        }
        active.push_back(e.uuid);
    }
    std::erase_if(gUseStart, [&](auto const& kv) { return std::find(active.begin(), active.end(), kv.first) == active.end(); });
    gTracker.retain(active);
    if (active.empty()) return 0;

    // Java `findTargetedPlayer`: the player hit first by the 2-block look ray (-1: none).
    std::vector<int> target(players.size(), -1);
    for (std::size_t i = 0; i < players.size(); ++i) {
        if (!players[i].using_) continue;
        std::vector<cf::Vec>     feet;
        std::vector<std::size_t> index;
        for (std::size_t j = 0; j < players.size(); ++j) {
            if (j == i || players[j].dim != players[i].dim) continue;
            feet.push_back(players[j].feet);
            index.push_back(j);
        }
        auto const& f   = players[i].feet;
        int         hit = cf::pickTarget({f.x, f.y + 1.62, f.z}, toVec(players[i].p->getViewVector(1.f)), feet);
        if (hit >= 0) target[i] = static_cast<int>(index[static_cast<std::size_t>(hit)]);
    }

    int                   links = 0;
    std::set<std::string> done;
    for (std::size_t i = 0; i < players.size(); ++i) {
        auto const& u = players[i];
        if (!u.using_ || done.contains(u.uuid)) continue;
        int j = target[i];
        // Touching tips: the target also uses a feather and looks back at us.
        if (j < 0 || !players[static_cast<std::size_t>(j)].using_ || target[static_cast<std::size_t>(j)] != static_cast<int>(i)) {
            gTracker.reset(u.uuid);
            continue;
        }
        auto const& t  = players[static_cast<std::size_t>(j)];
        auto        la = handFeather(*u.p);
        auto        lb = handFeather(*t.p);
        if (!la || !lb || cf::areLinked(infoOf(game::itemAt(*u.p, *la)), infoOf(game::itemAt(*t.p, *lb)))) {
            gTracker.reset(u.uuid);
            continue;
        }
        auto heart = cf::heartPos(u.feet, t.feet);
        if (gTick % cf::kParticleInterval == 0 && u.uuid <= t.uuid) {
            game::particles(u.dim, "minecraft:heart_particle", Vec3{heart.x, heart.y, heart.z}, 1, 0.1f);
        }
        if (gTracker.advance(u.uuid, t.uuid) < cf::kTwinningDuration) continue;

        bool linked = linkAt(*u.p, *la, *t.p, *lb);
        if (linked) {
            game::particles(u.dim, "minecraft:heart_particle", Vec3{heart.x, heart.y, heart.z}, 5, 0.3f);
            game::soundAround(u.dim, "block.enchanting_table.use", u.p->getFeetPos(), 1.f, 1.2f);
            ++links;
            log().info("feather linked: {} <-> {}", u.p->getRealName(), t.p->getRealName());
        }
        gTracker.reset(u.uuid);
        gTracker.reset(t.uuid);
        stopUsing(*u.p);
        stopUsing(*t.p);
        done.insert(u.uuid);
        done.insert(t.uuid);
    }
    return links;
}

LinkOutcome link(Player& a, Player& b, bool give) {
    if (&a == &b || uuidOf(a) == uuidOf(b)) return {LinkResult::SamePlayer, {}};
    if (arePartners(a, b)) return {LinkResult::AlreadyLinked, {}};
    auto la = linkFeather(a);
    auto lb = linkFeather(b);
    if (!la || !lb) {
        if (!give) return {LinkResult::NoFeather, (!la ? a : b).getRealName()};
        // Check both first so nothing is handed out when one of them has no room.
        std::optional<game::ItemLocation> fa = la ? std::nullopt : freeSlot(a);
        std::optional<game::ItemLocation> fb = lb ? std::nullopt : freeSlot(b);
        if (!la && !fa) return {LinkResult::NoSpace, a.getRealName()};
        if (!lb && !fb) return {LinkResult::NoSpace, b.getRealName()};
        auto item = game::makeItem(ids::id(ids::kTwinboundFeather));
        if (!la) {
            game::setItemAt(a, *fa, item);
            la = fa;
        }
        if (!lb) {
            game::setItemAt(b, *fb, item);
            lb = fb;
        }
    }
    if (!linkAt(a, *la, b, *lb)) return {LinkResult::AlreadyLinked, {}};
    log().info("feather linked: {} <-> {}", a.getRealName(), b.getRealName());
    return {LinkResult::Success, {}};
}

std::string targetIdOf(Player& partner) { return std::string(kTargetPrefix) + uuidOf(partner); }

// ---- target provider ----------------------------------------------------------------------------------

BS_REGISTER_TARGET_PROVIDER(service::TargetProvider{
    .name = "feather",
    .list =
        [](Player& player, core::WarpSource const& source, std::vector<core::WarpTarget>& out) {
            if (!listsPartners(source.kind)) return;
            auto& reqs = service::get<service::IRequirements>();
            for (auto* partner : partnersOf(player)) {
                auto             feet = partner->getFeetPos();
                core::WarpTarget t;
                t.id   = targetIdOf(*partner);
                t.kind = "player";
                t.name = partner->getRealName();
                t.pos  = {
                    static_cast<int>(partner->getDimensionId()),
                    static_cast<int>(std::floor(feet.x)),
                    static_cast<int>(std::floor(feet.y)),
                    static_cast<int>(std::floor(feet.z)),
                };
                t.groups      = {std::string(service::groups::kPlayers)};
                t.extra       = {{"twinboundFeather", true}};
                t.requirement = reqs.resolve(player, source, t);
                t.canAfford   = reqs.canAfford(player, t.requirement);
                out.push_back(std::move(t));
            }
        },
    .resolve = [](Player& player, std::string const& targetId) -> std::optional<service::Destination> {
        auto* partner = onlinePlayer(targetId.substr(kTargetPrefix.size()));
        if (!partner || !arePartners(player, *partner)) return std::nullopt;
        auto                 feet = partner->getFeetPos();
        service::Destination d;
        d.block = {
            static_cast<int>(partner->getDimensionId()),
            static_cast<int>(std::floor(feet.x)),
            static_cast<int>(std::floor(feet.y)),
            static_cast<int>(std::floor(feet.z)),
        };
        d.x   = feet.x;
        d.y   = feet.y;
        d.z   = feet.z;
        d.yaw = partner->getRotation().y;
        return d;
    },
    .owns     = [](std::string const& id) { return id.starts_with(kTargetPrefix); },
    .onWarped = [](Player& player, std::string const& targetId) {
        log().info("feather warp: {} -> {}", player.getRealName(), targetId);
    },
});

// ---- module --------------------------------------------------------------------------------------------

class FeatherModule {
public:
    void install() {
        mTick = ll::event::EventBus::getInstance().emplaceListener<ll::event::ServerLevelTickEvent>([](auto&) { tickRitual(); });
    }
    void uninstall() {
        if (mTick) ll::event::EventBus::getInstance().removeListener(mTick);
        mTick = nullptr;
        gTracker.clear();
        gSimulatedUse.clear();
        gUseStart.clear();
    }

private:
    ll::event::ListenerPtr mTick;
};

} // namespace bs::feature::feather

BS_REGISTER_MODULE(bs::feature::feather::FeatherModule);
