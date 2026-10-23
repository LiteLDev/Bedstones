#include "feature/teleport/Teleport.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "ll/api/chrono/GameChrono.h"
#include "ll/api/command/CommandRegistrar.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/ListenerBase.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/entity/utilities/leashable_utility/LeashableUtility.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPermissionLevel.h"
#include "mc/server/commands/ServerCommandOrigin.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionType.h"
#include "mc/world/phys/AABB.h"

#include "game/Blocks.h"
#include "game/Effects.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "net/Screens.h"
#include "service/TargetProviders.h"
#include "service/WarpEvents.h"

BS_REGISTER_CONFIG("teleports", bs::feature::teleport::TeleportsConfig);

namespace bs::feature::teleport {

namespace rules = core::teleport;
using service::Destination;
using service::WarpRequest;
using service::WarpResult;

namespace {

constexpr char const* kPortalSound    = "portal.travel";
constexpr char const* kPortalParticle = "minecraft:portal_directional";
constexpr int         kChunkLoadTicks = 100; ///< give up loading the destination after 5 s
constexpr int         kArrivalTicks   = 200; ///< cross-dimension: wait this long for the player to arrive
constexpr float       kSearchRadius   = 10.f;

int gGeneration = 0; ///< bumped on uninstall: pending warps stop

int dimOf(Actor const& a) { return static_cast<int>(a.getDimensionId()); }

Level* level() { return ll::service::getLevel().as_ptr(); }

template <class Fn>
void launch(Fn fn) {
    ll::coro::keepThis(std::move(fn)).launch(ll::thread::ServerThreadExecutor::getDefault());
}

/// Runs a command as the server in dimension `dim` (ticking areas for chunk loading).
void runCommand(int dim, std::string const& command) {
    auto* lv = level();
    if (!lv) return;
    ServerCommandOrigin origin("bedstones-teleport", lv->asServer(), CommandPermissionLevel::Owner, DimensionType(dim));
    auto out = ll::command::CommandRegistrar::getServerInstance().executeCommand(command, origin);
    if (out.mSuccessCount == 0) log().warn("teleport: command failed (ticking area limit?): {}", command);
}

// ---- world queries -----------------------------------------------------------------------------

bool suffocating(BlockSource& region, BlockPos const& pos) {
    auto const& b = region.getBlock(pos);
    return !b.isAir() && b.mCachedComponentData->mIsSolid;
}

bool loadedAround(int dim, core::GlobalPos const& pos) {
    auto* region = regionOf(dim);
    return region && region->hasChunksAt(game::blockPos(pos), 1, false);
}

/// The block standing at the record's position is a teleport target (Waystones' `isValidInLevel`).
/// When the block of the record's type is not registered at all (the unit providing it is absent),
/// only the record is checked.
bool targetBlockValid(core::WaystoneRecord const& r) {
    auto* region = regionOf(r.pos.dim);
    if (!region) return false;
    auto id = game::blockIdAt(*region, game::blockPos(r.pos));
    if (rules::isTeleportTargetPath(ids::pathOf(id))) return true;
    return game::blockWith(ids::id(rules::expectedBlockPath(r.type, r.style))) == nullptr;
}

Destination landingAt(core::WaystoneRecord const& r) {
    auto* region = regionOf(r.pos.dim);
    bool  loaded = region && region->hasChunksAt(game::blockPos(r.pos), 1, false);
    auto  base   = game::blockPos(r.pos);
    auto  space  = [&](int dx, int dz) {
        BlockPos at{base.x + dx, base.y, base.z + dz};
        return !suffocating(*region, at) && !suffocating(*region, BlockPos{at.x, at.y + 1, at.z});
    };
    auto landing = rules::findLanding(r.facing, r.type == core::types::kWarpPlate, loaded ? std::function<bool(int, int)>(space) : nullptr);
    Destination d;
    d.block     = r.pos;
    d.x         = static_cast<float>(r.pos.x + landing.dx) + 0.5f;
    d.y         = static_cast<float>(r.pos.y);
    d.z         = static_cast<float>(r.pos.z + landing.dz) + 0.5f;
    d.yaw       = rules::yawOf(landing.facing);
    d.targetUid = r.uid;
    return d;
}

std::optional<Destination> resolveTarget(Player& player, std::string const& targetId) {
    if (auto const* provider = service::providerFor(targetId)) {
        return provider->resolve ? provider->resolve(player, targetId) : std::nullopt;
    }
    auto* r = game::Waystones::get().find(targetId);
    if (!r) return std::nullopt;
    return landingAt(*r);
}

std::vector<Actor*> nearby(Player& player, float radius) {
    auto feet = player.getFeetBlockPos();
    AABB box{
        Vec3{static_cast<float>(feet.x) - radius, static_cast<float>(feet.y) - radius, static_cast<float>(feet.z) - radius},
        Vec3{static_cast<float>(feet.x) + 1 + radius, static_cast<float>(feet.y) + 1 + radius, static_cast<float>(feet.z) + 1 + radius}
    };
    std::vector<Actor*> out;
    for (auto* a : player.getDimensionBlockSource().getEntities(box, 0.f)) {
        if (a && a != &player && !a->isPlayer() && a->isAlive()) out.push_back(a);
    }
    return out;
}

// ---- warp context ------------------------------------------------------------------------------

struct Link {
    ActorUniqueID passenger;
    ActorUniqueID vehicle;
};

struct WarpContext {
    WarpRequest                request;
    ActorUniqueID              player{};
    int                        targetDim = 0;
    bool                       crossDim  = false;
    Destination                dest;
    core::WarpTarget           target;
    core::Requirement          requirement;
    std::vector<ActorUniqueID> pets;
    std::vector<ActorUniqueID> leashed;
};

struct Failure {
    std::string key;
    std::string fallback;
};

Failure missingWaystone() {
    return {"chat.waystones.waystone_missing", "The waystone you tried to teleport to is currently being moved or has gone missing."};
}

WarpResult refuse(Player& player, WarpRequest const& request, Failure const& f) {
    game::actionBar(player, "§4" + trFor(player, f.key, f.fallback));
    log().info("warp refused: {} -> {}: {}", player.getRealName(), targetName(request.targetId), f.key);
    return WarpResult::fail(f.key, f.fallback);
}

core::WarpTarget warpTarget(Player& player, std::string const& targetId, Destination const& dest) {
    core::WarpTarget t;
    t.id  = targetId;
    t.pos = dest.block;
    if (auto* r = game::Waystones::get().find(targetId)) {
        t.kind   = r->isSharestone() ? "sharestone" : r->type;
        t.name   = game::Waystones::get().playerData(player).effectiveName(r->uid, r->name);
        t.global = r->isGlobal();
    } else if (auto const* p = service::providerFor(targetId)) {
        t.kind = p->name;
    }
    return t;
}

bool itemStillHeld(Player& player, core::WarpSource const& source) {
    if (!source.isItem() || source.itemId.empty()) return true;
    return game::idOf(game::itemAt(player, game::handSlot(player, source.hand))) == source.itemId;
}

std::optional<Failure> requirementFailure(Player& player, WarpContext const& w) {
    if (w.request.skipRequirements || player.isCreative()) return std::nullopt;
    if (service::get<service::IRequirements>().canAfford(player, w.requirement)) return std::nullopt;
    if (!w.requirement.refuse.empty()) {
        auto const& r = w.requirement.refuse;
        if (r.starts_with('$')) return Failure{r.substr(1), r.substr(1)};
        return Failure{r, r};
    }
    return Failure{"chat.waystones.not_enough_xp", "You do not have enough experience to teleport."};
}

/// The checks Waystones repeats before and after loading the destination: the player, the source
/// waystone's range and the warp item.
std::optional<Failure> validatePlayer(Player& player, core::WarpSource const& source) {
    if (!player.isAlive()) return Failure{"chat.waystones.teleport_no_longer_valid", "The teleport could not be completed."};
    if (!sourceInRange(player, source)) {
        return Failure{"chat.waystones.source_waystone_out_of_range", "You moved too far away from the waystone."};
    }
    if (!itemStillHeld(player, source)) return Failure{"chat.waystones.source_item_missing", "You stopped holding the item to teleport."};
    return std::nullopt;
}

std::optional<Failure> validateLeashed(std::vector<Actor*> const& leashed, bool crossDim) {
    auto const& deny      = config::get<TeleportsConfig>().entityDenyList;
    bool        anyDenied = std::any_of(leashed.begin(), leashed.end(), [&](Actor* a) { return rules::isDenied(deny, a->getTypeName()); });
    if (auto e = rules::checkLeashed(transportLeashed(), static_cast<int>(leashed.size()), anyDenied, crossDim)) {
        auto [key, text] = rules::message(*e);
        return Failure{std::string(key), std::string(text)};
    }
    return std::nullopt;
}

/// Waystones' `validateTeleportStart` (after the context is built).
std::optional<Failure> validateStart(Player& player, WarpContext const& w, std::vector<Actor*> const& leashed) {
    if (auto f = validatePlayer(player, w.request.source)) return f;
    if (auto f = validateLeashed(leashed, w.crossDim)) return f;
    return requirementFailure(player, w);
}

/// Pending (destination loading) warps per player: a newer warp supersedes an older one, so a
/// player never pays or teleports twice for warps started during one load.
std::map<std::int64_t, unsigned>& pendingWarps() {
    static std::map<std::int64_t, unsigned> m;
    return m;
}

// ---- teleporting -------------------------------------------------------------------------------

void portalEffects(int dim, Vec3 const& feet, float volume) {
    Vec3 body{feet.x, feet.y + 1.f, feet.z};
    game::soundAround(dim, kPortalSound, body, volume);
    game::particles(dim, kPortalParticle, body, 128, 1.5f);
}

void zeroVerticalVelocity(Actor& a) {
    if (a.isPlayer() || a.isGliding()) return; // players: the teleport resets the client's motion
    const_cast<Vec3&>(a.getPosDelta()).y = 0.f;
}

/// Everything after the entities are in place: re-mount, re-leash, target effects, arrival.
struct Arrival {
    ActorUniqueID              player{};
    int                        dim = 0;
    Vec3                       at;
    std::string                targetUid;
    std::vector<Link>          links;
    std::vector<ActorUniqueID> leashed;
    std::vector<ActorUniqueID> others; ///< non-player entities that came along
};

void arrive(Arrival const& a, bool effectsAndArrival, bool relink) {
    auto* lv = level();
    if (!lv) return;
    auto* player = lv->getPlayer(a.player);
    auto* record = game::Waystones::get().find(a.targetUid);
    if (effectsAndArrival) {
        portalEffects(a.dim, a.at, 0.05f);
        if (record) {
            auto copy = *record;
            if (player) service::get<service::IArrival>().onArrived(*player, copy);
            for (auto id : a.others) {
                if (auto* e = lv->fetchEntity(id, false)) service::get<service::IArrival>().onArrived(*e, copy);
            }
        }
    }
    if (!relink) return;
    for (auto const& l : a.links) {
        auto* passenger = lv->fetchEntity(l.passenger, false);
        auto* vehicle   = lv->fetchEntity(l.vehicle, false);
        if (passenger && vehicle && passenger->getVehicle() != vehicle) passenger->startRiding(*vehicle, true);
    }
    if (player) {
        for (auto id : a.leashed) {
            auto* mob = lv->fetchEntity(id, false);
            if (mob && !(mob->isLeashed() && mob->getLeashHolder() == player->getOrCreateUniqueID())) {
                LeashableUtility::leash(*mob, *player, false, false);
            }
        }
    }
    for (auto id : a.others) {
        if (auto* e = lv->fetchEntity(id, false)) zeroVerticalVelocity(*e);
    }
}

/// Collects the player's mount tree: the root vehicle and every passenger riding it (recursively).
std::vector<Actor*> mountTree(Player& player, Actor*& root) {
    root = &player;
    while (auto* v = root->getVehicle()) root = v;
    std::vector<Actor*> riders;
    if (root == &player && !player.hasPassenger()) return riders;
    auto pos = root->getFeetPos();
    AABB box{Vec3{pos.x - 8, pos.y - 8, pos.z - 8}, Vec3{pos.x + 8, pos.y + 8, pos.z + 8}};
    std::set<Actor*> seen;
    for (auto* e : root->getDimensionBlockSource().getEntities(box, 2.f)) {
        if (!e || e == root || seen.contains(e)) continue;
        for (auto* v = e->getVehicle(); v; v = v->getVehicle()) {
            if (v == root) {
                riders.push_back(e);
                seen.insert(e);
                break;
            }
        }
    }
    if (root != &player && !seen.contains(&player)) riders.push_back(&player);
    return riders;
}

WarpResult complete(Player& player, WarpContext w) {
    auto& request = w.request;
    // validatePendingTeleport
    if (auto f = validatePlayer(player, request.source)) return refuse(player, request, *f);
    if (!regionOf(w.dest.block.dim)) return refuse(player, request, missingWaystone());
    if (!loadedAround(w.dest.block.dim, w.dest.block)) {
        return refuse(player, request, {"chat.waystones.destination_chunk_load_failed", "The destination could not be loaded."});
    }
    std::optional<Destination> dest;
    if (service::providerFor(request.targetId)) {
        dest = resolveTarget(player, request.targetId);
    } else if (auto* r = game::Waystones::get().find(request.targetId); r && targetBlockValid(*r)) {
        dest = landingAt(*r);
    }
    if (!dest || !loadedAround(dest->block.dim, dest->block)) return refuse(player, request, missingWaystone());
    // The target (or the player) may have changed dimension while the destination loaded.
    w.dest      = *dest;
    w.targetDim = dest->block.dim;
    w.crossDim  = w.targetDim != dimOf(player);
    {
        std::vector<Actor*> leashed;
        if (auto* lv = level()) {
            for (auto id : w.leashed) {
                if (auto* e = lv->fetchEntity(id, false)) leashed.push_back(e);
            }
        }
        if (auto f = validateLeashed(leashed, w.crossDim)) return refuse(player, request, *f);
        if (!rules::allowsTransport(transportPets(), w.crossDim)) w.pets.clear();
    }
    if (auto* region = regionOf(w.targetDim); region && (w.dest.y < region->getMinHeight() || w.dest.y >= region->getMaxHeight())) {
        return refuse(player, request, {"chat.waystones.destination_out_of_bounds", "The destination is outside the world bounds."});
    }
    if (auto f = requirementFailure(player, w)) return refuse(player, request, *f);
    if (!request.skipRequirements) service::get<service::IRequirements>().consume(player, w.requirement);

    auto* lv = level();
    if (!lv) return refuse(player, request, {"chat.waystones.teleport_failed", "The teleport failed."});

    // Who comes along: the mount tree (unless the requirements dismount), pets, leashed mobs.
    Actor*              root = &player;
    std::vector<Actor*> riders;
    if (w.requirement.dismount) {
        if (player.getVehicle()) player.stopRiding(true, false, false, true);
    } else {
        riders = mountTree(player, root);
    }
    std::set<Actor*> moving{root};
    moving.insert(riders.begin(), riders.end());
    Arrival arrival;
    arrival.player    = player.getOrCreateUniqueID();
    arrival.dim       = w.targetDim;
    arrival.at        = Vec3{w.dest.x, w.dest.y, w.dest.z};
    arrival.targetUid = w.dest.targetUid;
    for (auto* e : riders) {
        if (auto* v = e->getVehicle()) arrival.links.push_back({e->getOrCreateUniqueID(), v->getOrCreateUniqueID()});
    }
    std::vector<Actor*> extra;
    auto                take = [&](std::vector<ActorUniqueID> const& ids, bool leash) {
        for (auto id : ids) {
            auto* e = lv->fetchEntity(id, false);
            if (!e || !e->isAlive() || moving.contains(e)) continue;
            moving.insert(e);
            extra.push_back(e);
            if (leash) arrival.leashed.push_back(id);
        }
    };
    take(w.pets, false);
    take(w.leashed, true);
    for (auto* e : moving) {
        if (e != &player) arrival.others.push_back(e->getOrCreateUniqueID());
    }

    // Leashes do not survive a dimension change: detach them now, re-attach on arrival.
    if (w.crossDim) {
        for (auto id : arrival.leashed) {
            if (auto* mob = lv->fetchEntity(id, false)) LeashableUtility::unleash(*mob, LeashableUtility::UnleashReason::Removed);
        }
    }
    for (auto* e : riders) e->stopRiding(true, false, false, true);

    int  fromDim  = dimOf(player);
    Vec3 fromFeet = player.getFeetPos();
    portalEffects(fromDim, fromFeet, 0.01f);
    Vec3 feet{w.dest.x, w.dest.y, w.dest.z};
    for (auto* e : moving) {
        e->teleport(feet, DimensionType(w.targetDim), Vec2{e->getRotation().x, w.dest.yaw});
    }
    log().info("warp: {} -> {}", player.getRealName(), targetName(request.targetId));

    if (!w.crossDim) {
        arrive(arrival, true, false);
        int gen = gGeneration;
        launch([arrival, gen]() -> ll::coro::CoroTask<> {
            co_await ll::chrono::ticks(1);
            if (gen == gGeneration) arrive(arrival, false, true);
        });
    } else {
        int gen = gGeneration;
        launch([arrival, gen]() -> ll::coro::CoroTask<> {
            for (int i = 0; i < kArrivalTicks; ++i) {
                co_await ll::chrono::ticks(1);
                if (gen != gGeneration) co_return;
                auto* lv = level();
                auto* p  = lv ? lv->getPlayer(arrival.player) : nullptr;
                if (!p) co_return;
                if (dimOf(*p) == arrival.dim) break;
            }
            co_await ll::chrono::ticks(5); // let the other entities finish their dimension change
            if (gen == gGeneration) arrive(arrival, true, true);
        });
    }

    if (auto const* provider = service::providerFor(request.targetId); provider && provider->onWarped) {
        provider->onWarped(player, request.targetId);
    }
    service::notifyWarped(player, request, w.dest);
    return WarpResult::success();
}


// ---- the seam ------------------------------------------------------------------------------------

class WaystonesTeleport final : public service::ITeleport {
public:
    std::optional<Destination> resolve(Player& player, std::string const& targetId) override {
        return resolveTarget(player, targetId);
    }

    WarpResult warp(Player& player, WarpRequest const& request) override {
        auto dest = resolveTarget(player, request.targetId);
        if (!dest) return refuse(player, request, missingWaystone());

        WarpContext w;
        w.request   = request;
        w.player    = player.getOrCreateUniqueID();
        w.dest      = *dest;
        w.targetDim = dest->block.dim;
        w.crossDim  = w.targetDim != dimOf(player);
        w.target    = warpTarget(player, request.targetId, *dest);
        if (!request.skipRequirements) {
            w.requirement = service::get<service::IRequirements>().resolve(player, request.source, w.target);
        }
        auto leashed = findLeashed(player);
        if (rules::allowsTransport(transportPets(), w.crossDim)) {
            for (auto* pet : findPets(player)) w.pets.push_back(pet->getOrCreateUniqueID());
        }
        for (auto* mob : leashed) w.leashed.push_back(mob->getOrCreateUniqueID());

        if (auto f = validateStart(player, w, leashed)) return refuse(player, request, *f);
        if (auto taken = service::intercept(player, request)) return *taken;

        if (loadedAround(w.targetDim, w.dest.block)) {
            pendingWarps().erase(w.player.rawID); // supersedes a warp still loading its destination
            return complete(player, std::move(w));
        }

        // Load the destination (target chunk + its 4 neighbours) first, then finish.
        log().info("warp: loading destination of {} -> {}", player.getRealName(), targetName(request.targetId));
        // The result is "accepted"; errors after loading are shown to the player by complete().
        static unsigned tokens = 0;
        unsigned        token  = ++tokens;
        pendingWarps()[w.player.rawID] = token;
        int gen = gGeneration;
        launch([w = std::move(w), gen, token]() mutable -> ll::coro::CoroTask<> {
            co_await preloadDestination(w.dest.block);
            auto it = pendingWarps().find(w.player.rawID);
            if (it == pendingWarps().end() || it->second != token) co_return; // superseded
            pendingWarps().erase(it);
            if (gen != gGeneration) co_return;
            auto* lv = level();
            auto* p  = lv ? lv->getPlayer(w.player) : nullptr;
            if (!p) co_return;
            complete(*p, std::move(w));
        });
        return WarpResult::success();
    }
};

// ---- menus close when the player walks away from the source ---------------------------------------

class TeleportModule {
public:
    void install() {
        service::set<service::ITeleport>(std::make_unique<WaystonesTeleport>());
        auto& bus = ll::event::EventBus::getInstance();
        mTick     = bus.emplaceListener<ll::event::ServerLevelTickEvent>([this](auto&) {
            if (++mTicks % 4 != 0) return;
            auto* lv = level();
            if (!lv) return;
            lv->forEachPlayer([](Player& p) {
                auto* s = net::screens::current(p);
                if (!s || !s->context.is_object() || !s->context.contains("source")) return true;
                auto source = core::WarpSource::fromJson(s->context["source"]);
                if (!sourceInRange(p, source)) {
                    log().info("teleport: closed screen {} of {} (moved away from the source)", s->id, p.getRealName());
                    net::screens::close(p);
                }
                return true;
            });
        });
    }
    void uninstall() {
        ++gGeneration;
        if (mTick) ll::event::EventBus::getInstance().removeListener(mTick);
        mTick.reset();
        service::reset<service::ITeleport>();
    }

private:
    ll::event::ListenerPtr mTick;
    unsigned               mTicks = 0;
};

} // namespace

// ---- public helpers ------------------------------------------------------------------------------

rules::TransportMode transportPets() {
    return rules::transportModeFromString(config::get<TeleportsConfig>().transportPets, rules::TransportMode::Disabled);
}
rules::TransportMode transportLeashed() {
    return rules::transportModeFromString(config::get<TeleportsConfig>().transportLeashed, rules::TransportMode::Enabled);
}

ll::coro::CoroTask<bool> preloadDestination(core::GlobalPos pos) {
    if (loadedAround(pos.dim, pos)) co_return true;
    if (!regionOf(pos.dim)) co_return false;
    // Bedrock only loads chunks that something keeps loaded: a temporary ticking area around the
    // destination (radius 1 chunk covers the 4 neighbouring blocks' chunks) does that.
    static unsigned counter = 0;
    int             gen     = gGeneration;
    auto            name    = "bs_warp_" + std::to_string(++counter % 100000);
    runCommand(pos.dim, "tickingarea add circle " + std::to_string(pos.x) + " " + std::to_string(pos.y) + " " + std::to_string(pos.z) + " 1 " + name + " true");
    for (int i = 0; i < kChunkLoadTicks && !loadedAround(pos.dim, pos); ++i) {
        co_await ll::chrono::ticks(1);
        if (gen != gGeneration) break;
    }
    bool loaded = gen == gGeneration && loadedAround(pos.dim, pos);
    // Removed a little later so the chunks stay while the warp completes.
    launch([dim = pos.dim, name]() -> ll::coro::CoroTask<> {
        co_await ll::chrono::ticks(40);
        runCommand(dim, "tickingarea remove " + name);
    });
    co_return loaded;
}

BlockSource* regionOf(int dim) {
    auto* lv = level();
    if (!lv) return nullptr;
    auto d = lv->getOrCreateDimension(DimensionType(dim)).lock();
    if (!d) return nullptr;
    return &d->getBlockSourceFromMainChunkSource();
}

std::vector<Actor*> findPets(Player& player) {
    auto const&         deny = config::get<TeleportsConfig>().entityDenyList;
    auto const&         id   = player.getOrCreateUniqueID();
    std::vector<Actor*> out;
    for (auto* a : nearby(player, kSearchRadius)) {
        if (!a->isTame() || a->getOwnerId() != id) continue;
        if (a->isSitting() || a->isLeashed() || a->getVehicle()) continue;
        if (rules::isDenied(deny, a->getTypeName())) continue;
        out.push_back(a);
    }
    return out;
}

std::vector<Actor*> findLeashed(Player& player) {
    auto const&         id = player.getOrCreateUniqueID();
    std::vector<Actor*> out;
    for (auto* a : nearby(player, kSearchRadius)) {
        if (a->isLeashed() && a->getLeashHolder() == id) out.push_back(a);
    }
    return out;
}

bool sourceInRange(Player& player, core::WarpSource const& source) {
    if (!source.isBlock()) return true;
    core::GlobalPos pos = source.fromPos;
    if (!source.fromUid.empty()) {
        auto* r = game::Waystones::get().find(source.fromUid);
        if (!r) return false;
        pos = r->pos;
    } else if (source.fromPos == core::GlobalPos{}) {
        return true; // no source block given
    }
    if (pos.dim != dimOf(player)) return false;
    auto eye = player.getHeadPos();
    return rules::canInteractWithBlock(eye.x, eye.y, eye.z, pos.x, pos.y, pos.z, rules::interactionRange(player.isCreative()), rules::kSourceBuffer);
}

std::string targetName(std::string const& targetId) {
    if (auto* r = game::Waystones::get().find(targetId); r && !r->name.empty()) return r->name;
    return targetId;
}

} // namespace bs::feature::teleport

BS_REGISTER_MODULE(bs::feature::teleport::TeleportModule);
