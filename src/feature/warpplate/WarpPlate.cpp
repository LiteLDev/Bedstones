#include "feature/warpplate/WarpPlate.h"

#include <algorithm>
#include <cmath>
#include <random>
#include <set>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerDestroyBlockEvent.h"
#include "ll/api/event/player/PlayerInteractBlockEvent.h"
#include "ll/api/event/player/PlayerPlaceBlockEvent.h"
#include "ll/api/event/world/BlockChangedEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/ActorType.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/item/ItemTag.h"
#include "mc/world/item/enchanting/Enchant.h"
#include "mc/world/item/enchanting/EnchantUtils.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/Spawner.h"
#include "mc/world/level/block/Block.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/world/level/dimension/DimensionType.h"
#include "mc/world/phys/AABB.h"

#include "core/warpplate/WarpPlateLogic.h"
#include "game/Blocks.h"
#include "game/Effects.h"
#include "game/Ids.h"
#include "game/Waystones.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "service/Services.h"
#include "service/WarpEvents.h"

BS_REGISTER_CONFIG("warpPlate", bs::feature::warpplate::WarpPlateConfig);

namespace bs::feature::warpplate {

namespace logic = core::warpplate;
using core::WaystoneRecord;

namespace {

constexpr std::uint64_t kInteractThrottle = 5; ///< ticks between two handled clicks of one player
constexpr float         kReachSq          = 8.f * 8.f;

std::string idOfSlot(std::optional<game::SlotItem> const& s) { return s ? s->id : std::string{}; }

std::string attunementOf(game::SlotItem const& s) {
    auto it = s.user.find(std::string(ids::kNbtAttunement));
    return it == s.user.end() ? std::string{} : it->second;
}

int countOf(game::Container const& c, std::string_view id) {
    int n = 0;
    for (auto const& s : c) {
        if (s && s->id == id) n += s->count;
    }
    return n;
}

bool hasModifier(game::Container const& c, std::string_view id) {
    for (std::size_t i = 1; i < c.size(); ++i) {
        if (c[i] && c[i]->id == id) return true;
    }
    return false;
}

BlockSource* regionOf(int dim) {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    auto dimension = level->getDimension(dim).lock();
    if (!dimension) return nullptr;
    return &dimension->getBlockSourceFromMainChunkSource();
}

Vec3 center(core::GlobalPos const& p, float dy = 0.5f) {
    return Vec3{static_cast<float>(p.x) + 0.5f, static_cast<float>(p.y) + dy, static_cast<float>(p.z) + 0.5f};
}

std::int64_t actorKey(Actor const& a) { return a.getOrCreateUniqueID().rawID; }

Player* playerById(std::int64_t id) {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    ActorUniqueID uid;
    uid.rawID = id;
    return level->getPlayer(uid);
}

std::mt19937& rng() {
    static std::mt19937 r{std::random_device{}()};
    return r;
}

/// Approximation of Java's `hasNeighborSignal`: a neighbour that is a powered redstone component.
bool isPowered(BlockSource& region, BlockPos const& pos) {
    static constexpr int kDirs[6][3] = {
        {1,  0,  0 },
        {-1, 0,  0 },
        {0,  1,  0 },
        {0,  -1, 0 },
        {0,  0,  1 },
        {0,  0,  -1}
    };
    for (auto const& d : kDirs) {
        BlockPos    n{pos.x + d[0], pos.y + d[1], pos.z + d[2]};
        auto const& b    = region.getBlock(n);
        auto        name = b.getTypeName();
        if (name == "minecraft:redstone_block" || name == "minecraft:redstone_torch"
            || name == "minecraft:powered_repeater" || name == "minecraft:powered_comparator") {
            return true;
        }
        if (game::intState(b, "redstone_signal").value_or(0) > 0) return true;
        if (game::intState(b, "button_pressed_bit").value_or(0) > 0) return true;
        if (game::intState(b, "powered_bit").value_or(0) > 0) return true;
        if (name == "minecraft:lever" && game::intState(b, "open_bit").value_or(0) > 0) return true;
    }
    return false;
}

/// Attuned / crumbling shards of the container that point at another plate (Waystones'
/// `getTargetAttunementStack` input), plus the quartz / spider eye flags.
std::vector<logic::Candidate>
candidatesOf(game::Container const& c, std::string const& self, bool& quartz, bool& spider) {
    std::vector<logic::Candidate> out;
    quartz = spider = false;
    for (std::size_t i = 0; i < c.size(); ++i) {
        if (!c[i]) continue;
        auto kind = logic::shardKindOf(c[i]->id);
        if (kind == logic::ShardKind::Attuned || kind == logic::ShardKind::Crumbling) {
            auto target = attunementOf(*c[i]);
            if (!target.empty() && target != self)
                out.push_back({static_cast<int>(i), kind == logic::ShardKind::Crumbling, target});
        } else if (c[i]->id == "minecraft:quartz") {
            quartz = true;
        } else if (c[i]->id == "minecraft:spider_eye") {
            spider = true;
        }
    }
    return out;
}

/// A target record that can be warped to (exists and is not a silk-touched plate in transit).
core::WaystoneRecord* liveTarget(std::string const& uid) {
    auto* r = game::Waystones::get().find(uid);
    return r && !r->transient ? r : nullptr;
}

bool deniedEntity(Actor const& a) { return a.getEntityTypeId() == ActorType::WitherBoss; }

std::string trP(Player const* p, std::string_view key, std::string_view fallback) {
    return p ? trFor(*p, key, fallback) : tr(key, fallback);
}

ItemStack stackWithLore(game::SlotItem const& item) {
    auto stack = game::toStack(item);
    auto kind  = logic::shardKindOf(item.id);
    if (kind == logic::ShardKind::Attuned || kind == logic::ShardKind::Crumbling) {
        game::setDisplay(stack, {}, shardLore(nullptr, item.id, attunementOf(item)));
    } else if (isPlateBlock(item.id)) {
        if (auto it = item.user.find(std::string(ids::kNbtWaystone)); it != item.user.end()) {
            game::setDisplay(stack, {}, {galacticName(it->second)});
        }
    }
    return stack;
}

void spawnItem(core::GlobalPos const& pos, game::SlotItem const& item) {
    auto* region = regionOf(pos.dim);
    if (!region) return;
    auto stack = stackWithLore(item);
    if (stack.isNull()) return;
    region->getLevel().getSpawner().spawnItem(*region, stack, nullptr, center(pos), 10);
}

char const* statusName(int s) {
    static constexpr char const* kNames[] =
        {"empty", "idle", "attuning", "warping", "warping_invalid", "redstone_disabled", "locked"};
    return s >= 0 && s < 7 ? kNames[s] : "?";
}

} // namespace

// ---- strings ----------------------------------------------------------------------------------------

std::string plateBlockId() { return ids::id(ids::kWarpPlate); }
bool        isPlateBlock(std::string_view blockId) { return blockId == plateBlockId(); }

std::string galacticName(std::string const& uid) {
    auto ident = logic::galacticIdentifier(uid);
    // Waystones draws the identifier in the Standard Galactic ("alt") font: the resource pack's
    // glyph page E1 holds it.
    return std::string("§") + logic::formattingCode(logic::chatFormattingId(logic::colorIndex(ident))) + logic::galacticText(ident);
}

std::vector<std::string> shardLore(Player const* player, std::string_view itemId, std::string const& target) {
    std::vector<std::string> lore;
    auto*                    rec = target.empty() ? nullptr : game::Waystones::get().find(target);
    if (rec && !rec->transient) {
        lore.push_back(rec->type == core::types::kWarpPlate ? galacticName(rec->uid) : "§d" + rec->name);
        lore.push_back(
            "§7"
            + trP(
                player,
                "tooltip.waystones.attuned_shard.plug_into_warp_plate",
                "Insert this shard into a warp plate."
            )
        );
    } else {
        lore.push_back(
            "§7" + trP(player, "tooltip.waystones.attuned_shard.attunement_lost", "This shard has lost its attunement.")
        );
    }
    if (logic::shardKindOf(itemId) == logic::ShardKind::Crumbling) {
        lore.push_back(
            "§f§o"
            + trP(
                player,
                "tooltip.waystones.attuned_shard.attunement_crumbling",
                "It will be consumed on next teleport."
            )
        );
    }
    return lore;
}

ItemStack makeAttunedShard(std::string const& target, bool crumbling) {
    game::SlotItem s{
        ids::id(crumbling ? ids::kCrumblingAttunedShard : ids::kAttunedShard),
        1,
        0,
        {{std::string(ids::kNbtAttunement), target}}
    };
    return stackWithLore(s);
}

// ---- module -------------------------------------------------------------------------------------------

WarpPlates& WarpPlates::get() {
    static WarpPlates instance;
    return instance;
}

core::WaystoneRecord* WarpPlates::onPlaced(
    Player*                           placer,
    core::GlobalPos const&            pos,
    std::optional<std::string> const& itemUid,
    bool                              dormantShard
) {
    auto& ws = game::Waystones::get();
    if (!ws.available()) return nullptr;
    if (auto* existing = ws.findAt(pos)) {
        if (existing->type == core::types::kWarpPlate && !existing->transient) return existing;
        if (existing->type != core::types::kWarpPlate) {
            ws.remove(std::string(existing->uid)); // a stale record of another block
        } else if (!itemUid || *itemUid != existing->uid) {
            // A silk-touched plate in transit still indexed here: park it off-world so the new
            // plate owns this position (its item restores it wherever it is placed).
            ws.move(std::string(existing->uid), core::GlobalPos{-1, 0, -(1 << 30), 0});
        }
    }
    if (itemUid) {
        if (auto* r = ws.find(*itemUid); r && r->type == core::types::kWarpPlate && r->transient) {
            ws.move(r->uid, pos);
            r            = ws.find(*itemUid);
            r->transient = false;
            ws.markDirty(r->uid);
            log().info("warp plate restored: {} by {}", r->name, placer ? placer->getRealName() : std::string("?"));
            return r;
        }
    }
    WaystoneRecord r;
    r.type       = std::string(core::types::kWarpPlate);
    r.style      = std::string(ids::kWarpPlate);
    r.pos        = pos;
    r.origin     = placer ? core::Origin::Player : core::Origin::Unknown;
    r.visibility = core::Visibility::ShardOnly;
    if (placer) {
        r.ownerUid  = game::Waystones::playerKey(*placer);
        r.ownerName = placer->getRealName();
        r.facing    = game::opposite(game::playerFacing(*placer));
    }
    r.name = service::get<service::INameGenerator>().generate(core::Origin::Player, {});
    if (dormantShard) {
        game::Container c{};
        c[0] = game::SlotItem{ids::id(ids::kDormantShard), 1, 0, {}};
        game::setContainer(r, c);
    }
    auto* created = ws.create(std::move(r));
    if (created) {
        log().info(
            "warp plate placed: {} ({}) at {} {} {} by {}",
            created->name,
            logic::galacticIdentifier(created->uid),
            pos.x,
            pos.y,
            pos.z,
            placer ? placer->getRealName() : std::string("?")
        );
    }
    return created;
}

bool WarpPlates::insertShard(Player& player, std::string const& uid, game::ItemLocation loc) {
    auto& ws  = game::Waystones::get();
    auto* rec = ws.find(uid);
    if (!rec) return false;
    auto const& stack = game::itemAt(player, loc);
    auto        id    = game::idOf(stack);
    if (!logic::isWarpShard(id)) return false;
    auto c = game::containerOf(*rec);
    if (c[0]) return false;
    auto item = game::toSlotItem(stack);
    if (!item) return false;
    item->count = 1;
    c[0]        = *item;
    game::setContainer(*rec, c);
    ws.markDirty(uid);
    game::consumeAt(player, loc, 1, true);
    mRt[uid].attune = 0;
    log().info("warp plate shard inserted: {} into {} by {}", id, rec->name, player.getRealName());
    return true;
}

bool WarpPlates::ejectShard(std::string const& uid) {
    auto& ws  = game::Waystones::get();
    auto* rec = ws.find(uid);
    if (!rec) return false;
    auto c = game::containerOf(*rec);
    if (!c[0]) return false;
    auto item = *c[0];
    c[0].reset();
    game::setContainer(*rec, c);
    ws.markDirty(uid);
    spawnItem(rec->pos, item);
    log().info("warp plate shard ejected: {} from {}", item.id, rec->name);
    return true;
}

void WarpPlates::noteBreak(core::GlobalPos const& pos, BreakInfo info) {
    info.tick          = mNow;
    mBreaks[pos.key()] = info;
}

void WarpPlates::onRemoved(WaystoneRecord const& record, std::optional<BreakInfo> info) {
    auto& ws  = game::Waystones::get();
    auto  uid = record.uid;
    auto  pos = record.pos;
    mRt.erase(uid);
    Drop drop{pos, {}};
    bool silk = false;
    if (!info || !info->creative) {
        // Java drops the container whenever the block goes away (not for creative breaks).
        for (auto const& s : game::containerOf(record)) {
            if (s) drop.items.push_back(*s);
        }
    }
    if (info && !info->creative) {
        if (info->pickaxe) {
            game::SlotItem plate{plateBlockId(), 1, 0, {}};
            if (info->silk) {
                plate.user[std::string(ids::kNbtWaystone)] = uid;
                silk                                       = true;
            }
            drop.items.push_back(plate);
        }
    }
    if (!drop.items.empty()) mDrops.push_back(std::move(drop));
    log().info("warp plate broken: {} (silk touch: {})", record.name, silk);
    if (silk) {
        if (auto* r = ws.find(uid)) {
            r->transient = true;
            r->extra.erase("container");
            ws.markDirty(uid);
        }
    } else {
        ws.remove(uid);
    }
}

int WarpPlates::useTimeOf(WaystoneRecord const& record) const {
    auto c = game::containerOf(record);
    return logic::useTime(
        config::get<WarpPlateConfig>().warpPlateUseTime,
        countOf(c, "minecraft:amethyst_shard"),
        countOf(c, "minecraft:slime_ball")
    );
}

int WarpPlates::statusOf(std::string const& uid) const {
    auto it = mRt.find(uid);
    return it == mRt.end() ? 0 : it->second.status;
}

void WarpPlates::markCooldown(std::string const& uid, std::int64_t actorId) {
    auto& rt              = mRt[uid];
    rt.cooldowns[actorId] = std::max(1, config::get<WarpPlateConfig>().warpPlateCooldownTime);
    rt.sessions.erase(actorId);
    rt.valid.erase(actorId);
}

bool WarpPlates::onCooldown(std::string const& uid, std::int64_t actorId) const {
    auto it = mRt.find(uid);
    return it != mRt.end() && it->second.cooldowns.contains(actorId);
}

// ---- warping ---------------------------------------------------------------------------------------------

bool WarpPlates::warpEntity(Actor& actor, std::string const& uid) {
    auto& ws  = game::Waystones::get();
    auto* rec = ws.find(uid);
    if (!rec) return false;
    auto    c      = game::containerOf(*rec);
    auto&   rt     = mRt[uid];
    bool    quartz = false, spider = false;
    auto    candidates = candidatesOf(c, uid, quartz, spider);
    auto    choice     = logic::chooseTarget(candidates, quartz, spider, rt.lastSlot, [](int n) {
        return std::uniform_int_distribution<int>(0, n - 1)(rng());
    });
    Player* player     = actor.isPlayer() ? static_cast<Player*>(&actor) : nullptr;
    if (!choice) {
        if (player) {
            game::actionBar(
                *player,
                "§4"
                    + trFor(
                        *player,
                        "chat.waystones.warp_plate_has_no_target",
                        "This warp plate is not bound to another warp plate yet."
                    )
            );
        }
        return false;
    }
    auto* target = liveTarget(choice->target);
    if (!target) {
        if (player) {
            game::actionBar(
                *player,
                "§4"
                    + trFor(
                        *player,
                        "chat.waystones.warp_plate_has_invalid_target",
                        "The shard inside this warp plate has lost its attunement."
                    )
            );
        }
        return false;
    }
    std::string targetUid  = target->uid;
    std::string targetName = target->name;
    std::string fromName   = rec->name;
    std::string shardId    = c[static_cast<std::size_t>(choice->slot)]->id;
    std::string who;
    bool        creative = player && player->isCreative();
    if (player) {
        service::WarpRequest req;
        req.source.kind    = core::WarpSourceKind::WarpPlate;
        req.source.fromUid = uid;
        req.source.fromPos = rec->pos;
        req.source.itemId  = shardId;
        req.targetId       = targetUid;
        auto res           = service::get<service::ITeleport>().warp(*player, req);
        if (!res.ok) return false;
        who = player->getRealName();
    } else {
        auto from    = actor.getPosition();
        int  fromDim = static_cast<int>(actor.getDimensionId());
        auto dest    = target->pos;
        actor.teleport(center(dest, 0.2f), DimensionType(dest.dim));
        markCooldown(targetUid, actorKey(actor));
        game::soundAround(fromDim, "mob.endermen.portal", from, 0.1f, 1.f);
        game::soundAround(dest.dim, "mob.endermen.portal", center(dest), 0.5f, 1.f);
        if (auto* t = ws.find(targetUid)) service::get<service::IArrival>().onArrived(actor, *t);
        who = actor.getTypeName();
    }
    // Single-use shards crumble after a successful warp (not for creative players).
    if (choice->singleUse && !creative) {
        if (auto* r = ws.find(uid)) {
            auto  cc = game::containerOf(*r);
            auto& s  = cc[static_cast<std::size_t>(choice->slot)];
            if (s && s->id == shardId && attunementOf(*s) == targetUid) {
                if (--s->count <= 0) s.reset();
                game::setContainer(*r, cc);
                ws.markDirty(uid);
                log().info("warp plate crumbling shard consumed: {}", r->name);
            }
        }
    }
    log().info("warp plate warp: {} from {} to {}", who, fromName, targetName);
    return true;
}

void WarpPlates::tickPlate(std::string const& uid) {
    auto& ws  = game::Waystones::get();
    auto* rec = ws.find(uid);
    if (!rec) return;
    auto* region = regionOf(rec->pos.dim);
    if (!region) return;
    auto pos = game::blockPos(rec->pos);
    if (!region->hasChunksAt(pos, 0, false)) return;
    auto const& block = region->getBlock(pos);
    if (!isPlateBlock(block.getTypeName())) return;
    auto&       rt  = mRt[uid];
    auto const& cfg = config::get<WarpPlateConfig>();

    // Attunement: a dormant shard becomes bound to this plate, then a self-bound shard is ejected.
    auto c       = game::containerOf(*rec);
    bool changed = false;
    if (logic::shardKindOf(idOfSlot(c[0])) == logic::ShardKind::Dormant) {
        if (++rt.attune >= logic::kAttunementTicks) {
            rt.attune = 0;
            c[0]      = game::SlotItem{ids::id(ids::kAttunedShard), 1, 0, {{std::string(ids::kNbtAttunement), uid}}};
            changed   = true;
            log().info("warp plate attuned: {}", rec->name);
        }
    } else {
        rt.attune = 0;
    }
    if (c[0] && logic::shardKindOf(c[0]->id) == logic::ShardKind::Attuned && attunementOf(*c[0]) == uid) {
        spawnItem(rec->pos, *c[0]);
        c[0].reset();
        changed = true;
        game::soundAround(rec->pos.dim, "mob.chicken.plop", center(rec->pos), 1.f, 1.f);
        game::particle(rec->pos.dim, "minecraft:wind_explosion_emitter", center(rec->pos));
        log().info("warp plate shard ejected: attuned shard from {}", rec->name);
    }
    if (changed) {
        game::setContainer(*rec, c);
        ws.markDirty(uid);
        rec = ws.find(uid);
        if (!rec) return;
    }

    auto kind     = logic::shardKindOf(idOfSlot(c[0]));
    bool redstone = hasModifier(c, "minecraft:redstone_torch") && isPowered(*region, pos);
    auto status   = logic::idleStatus(kind, redstone);

    // Entities on top (Java: any entity intersecting the block cell).
    AABB cell{
        Vec3{static_cast<float>(pos.x),     static_cast<float>(pos.y),     static_cast<float>(pos.z)    },
        Vec3{static_cast<float>(pos.x + 1), static_cast<float>(pos.y + 1), static_cast<float>(pos.z + 1)}
    };
    std::vector<Actor*>    onTop;
    std::set<std::int64_t> onTopIds;
    auto                   consider = [&](Actor* a) {
        if (!a || !a->isAlive() || static_cast<int>(a->getDimensionId()) != rec->pos.dim) return;
        auto const& box = a->getAABB();
        if (box.max.x <= cell.min.x || box.min.x >= cell.max.x || box.max.y <= cell.min.y || box.min.y >= cell.max.y
            || box.max.z <= cell.min.z || box.min.z >= cell.max.z) {
            return;
        }
        if (!onTopIds.insert(actorKey(*a)).second) return;
        onTop.push_back(a);
    };
    for (auto* a : region->getEntities(cell, 0.f)) consider(a);
    // Players are not always part of the entity query (simulated players): check them directly.
    for (auto* p : mTickPlayers) consider(p);
    // Cooldowns: reset while standing on the plate, count down while off it.
    for (auto it = rt.cooldowns.begin(); it != rt.cooldowns.end();) {
        if (onTopIds.contains(it->first)) it->second = std::max(1, cfg.warpPlateCooldownTime);
        else --it->second;
        it = it->second <= 0 ? rt.cooldowns.erase(it) : std::next(it);
    }
    std::erase_if(rt.sessions, [&](auto const& kv) { return !onTopIds.contains(kv.first); });
    std::erase_if(rt.valid, [&](auto const& kv) { return !rt.sessions.contains(kv.first); });

    if (status == logic::Status::Idle && logic::hasPotentialTarget(kind)) {
        int                       useTime = useTimeOf(*rec);
        std::vector<std::int64_t> due;
        for (auto* a : onTop) {
            auto id = actorKey(*a);
            if (a->getVehicle() || deniedEntity(*a)) {
                rt.sessions.erase(id);
                continue;
            }
            if (rt.cooldowns.contains(id)) continue;
            auto [it, fresh] = rt.sessions.try_emplace(id, 0);
            if (fresh) {
                // Status for this entity: a live target (spider eye preference applied) it can afford.
                bool quartz = false, spider = false;
                auto cands  = candidatesOf(c, uid, quartz, spider);
                bool single = false;
                for (auto const& cd : cands) single = single || cd.singleUse;
                bool valid = false;
                for (auto const& cd : cands) {
                    if (spider && single && !cd.singleUse) continue;
                    auto* target = liveTarget(cd.target);
                    if (!target) continue;
                    valid = true;
                    if (a->isPlayer()) {
                        auto&            p = static_cast<Player&>(*a);
                        core::WarpSource src;
                        src.kind    = core::WarpSourceKind::WarpPlate;
                        src.fromUid = uid;
                        src.fromPos = rec->pos;
                        core::WarpTarget wt;
                        wt.id    = target->uid;
                        wt.kind  = target->type;
                        wt.name  = target->name;
                        wt.pos   = target->pos;
                        auto& rq = service::get<service::IRequirements>();
                        valid    = p.isCreative() || rq.canAfford(p, rq.resolve(p, src, wt));
                    }
                    break;
                }
                rt.valid[id] = valid;
            }
            if (it->second > useTime) due.push_back(id);
            else ++it->second;
        }
        for (auto id : due) {
            rt.sessions[id] = 0; // Java resets the counter after every attempt
            // Looked up again: an earlier warp may have moved (re-created) other actors.
            ActorUniqueID key;
            key.rawID = id;
            if (auto* actor = region->getLevel().fetchEntity(key, false); actor && actor->isAlive())
                warpEntity(*actor, uid);
        }
        if (!rt.sessions.empty()) {
            bool anyValid = false;
            for (auto const& [id, v] : rt.valid) anyValid = anyValid || v;
            status = anyValid ? logic::Status::Warping : logic::Status::WarpingInvalid;
        }
    } else {
        rt.sessions.clear();
        rt.valid.clear();
    }

    // Block state (status + galactic colour).
    rec = ws.find(uid);
    if (!rec) return;
    auto&       rtNow  = rt;
    int         st     = static_cast<int>(status);
    int         color  = logic::colorIndex(logic::galacticIdentifier(uid));
    auto const& now    = region->getBlock(pos);
    auto        curSt  = game::intState(now, ids::kStateStatus).value_or(-1);
    auto        curCol = game::intState(now, kColorState).value_or(-1);
    if (curSt != st || curCol != color) {
        if (auto const* b = game::blockWith(
                plateBlockId(),
                {
                    {ids::kStateStatus, st   },
                    {kColorState,       color}
        }
            )) {
            game::setBlock(*region, pos, *b);
        }
    }
    if (rtNow.status != st) {
        log().info("warp plate status: {} {}", rec->name, statusName(st));
        rtNow.status = st;
    }

    // Particles (fewer than Java's per-frame client effects: each one is a packet here).
    if (mNow % 2 == 0) {
        auto dim = rec->pos.dim;
        auto at  = [&](float h) {
            std::uniform_real_distribution<float> d(0.f, 1.f);
            return Vec3{
                static_cast<float>(pos.x) + d(rng()),
                static_cast<float>(pos.y) + d(rng()) * h,
                static_cast<float>(pos.z) + d(rng())
            };
        };
        switch (status) {
        case logic::Status::Warping:
            for (int i = 0; i < 4; ++i) game::particle(dim, "minecraft:mob_portal", at(2.f));
            for (int i = 0; i < 2; ++i) game::particle(dim, "minecraft:basic_portal_particle", at(2.f));
            break;
        case logic::Status::WarpingInvalid:
            for (int i = 0; i < 2; ++i) game::particle(dim, "minecraft:basic_smoke_particle", at(0.5f));
            break;
        case logic::Status::RedstoneDisabled:
            if (mNow % 8 == 0) game::particle(dim, "minecraft:redstone_wire_dust_particle", at(0.3f));
            break;
        case logic::Status::Attuning:
            game::particle(dim, "minecraft:blue_flame_particle", at(0.5f));
            game::particle(dim, "minecraft:conduit_particle", at(1.f));
            break;
        default:
            break;
        }
    }
}

// ---- deferred work -----------------------------------------------------------------------------------------

void WarpPlates::runPending() {
    auto pending = std::move(mPending);
    mPending.clear();
    auto& ws = game::Waystones::get();
    for (auto const& a : pending) {
        auto* player = playerById(a.player);
        auto* rec    = ws.find(a.uid);
        if (!player || !rec || !player->isAlive()) continue;
        if (static_cast<int>(player->getDimensionId()) != rec->pos.dim
            || player->getPosition().distanceToSqr(center(rec->pos)) > kReachSq) {
            continue;
        }
        switch (a.kind) {
        case Pending::Kind::Insert:
            insertShard(*player, a.uid, {game::ItemLocation::Kind::Inventory, a.slot});
            break;
        case Pending::Kind::Eject:
            ejectShard(a.uid);
            break;
        case Pending::Kind::Edit:
            log().info("warp plate edit: {} by {}", rec->name, player->getRealName());
            service::get<service::IEditScreen>().open(*player, a.uid);
            break;
        }
    }
}

void WarpPlates::spawnDrops() {
    auto drops = std::move(mDrops);
    mDrops.clear();
    for (auto const& d : drops) {
        for (auto const& item : d.items) spawnItem(d.pos, item);
    }
}

void WarpPlates::checkOrphans() {
    auto orphans = std::move(mOrphans);
    mOrphans.clear();
    auto& ws = game::Waystones::get();
    for (auto const& pos : orphans) {
        if (auto* r = ws.findAt(pos); r && !r->transient) continue;
        auto* region = regionOf(pos.dim);
        if (!region || !isPlateBlock(game::blockIdAt(*region, game::blockPos(pos)))) continue;
        // A recent placement click of a plate item nearby: that player placed it.
        Player*                    placer = nullptr;
        std::optional<std::string> itemUid;
        for (auto it = mPlacing.begin(); it != mPlacing.end(); ++it) {
            if (mNow - it->second.tick > 4) continue;
            auto* p = playerById(it->first);
            if (p && static_cast<int>(p->getDimensionId()) == pos.dim
                && p->getPosition().distanceToSqr(center(pos)) <= kReachSq * 2) {
                placer  = p;
                itemUid = it->second.uid;
                mPlacing.erase(it);
                break;
            }
        }
        onPlaced(placer, pos, itemUid, placer != nullptr);
    }
}

void WarpPlates::refreshLore() {
    auto level = ll::service::getLevel();
    if (!level) return;
    level->forEachPlayer([](Player& p) {
        if (p.isClientSide()) return true;
        int size = 36;
        for (int i = 0; i <= size; ++i) {
            game::ItemLocation loc = i < size ? game::ItemLocation{game::ItemLocation::Kind::Inventory, i}
                                              : game::ItemLocation{game::ItemLocation::Kind::Offhand, 0};
            auto const&        s   = game::itemAt(p, loc);
            if (s.isNull()) continue;
            auto id   = game::idOf(s);
            auto kind = logic::shardKindOf(id);
            if (kind != logic::ShardKind::Attuned && kind != logic::ShardKind::Crumbling) continue;
            auto lore = shardLore(&p, id, game::userString(s, ids::kNbtAttunement).value_or(""));
            if (s.getCustomLore() == lore) continue;
            ItemStack copy = s;
            game::setDisplay(copy, {}, lore);
            game::setItemAt(p, loc, copy);
        }
        return true;
    });
}

void WarpPlates::tick() {
    ++mNow;
    auto& ws = game::Waystones::get();
    if (!ws.available()) return;
    if (!mPending.empty()) runPending();
    if (!mOrphans.empty()) checkOrphans();
    if (!mDrops.empty()) spawnDrops();
    std::vector<std::string> uids;
    for (auto* r : ws.ofType(core::types::kWarpPlate)) {
        if (!r->transient) uids.push_back(r->uid);
    }
    mTickPlayers.clear();
    if (!uids.empty()) {
        if (auto level = ll::service::getLevel()) {
            level->forEachPlayer([this](Player& p) {
                if (!p.isClientSide()) mTickPlayers.push_back(&p);
                return true;
            });
        }
    }
    for (auto const& uid : uids) tickPlate(uid);
    mTickPlayers.clear();
    if (mNow % 20 == 0) {
        refreshLore();
        std::erase_if(mBreaks, [this](auto const& kv) { return mNow - kv.second.tick > 40; });
        std::erase_if(mPlacing, [this](auto const& kv) { return mNow - kv.second.tick > 40; });
        std::erase_if(mThrottle, [this](auto const& kv) { return mNow - kv.second > 40; });
        std::erase_if(mRt, [&ws](auto const& kv) { return ws.find(kv.first) == nullptr; });
    }
}

void WarpPlates::install() {
    auto& bus = ll::event::EventBus::getInstance();
    mListeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([this](auto&) { tick(); }));

    mListeners.push_back(bus.emplaceListener<ll::event::PlayerInteractBlockEvent>(
        [this](ll::event::PlayerInteractBlockEvent& ev) {
            auto& player = ev.self();
            if (player.isClientSide() || ev.isCancelled()) return;
            auto& region = player.getDimensionBlockSource();
            auto  pos    = ev.blockPos();
            if (!isPlateBlock(region.getBlock(pos).getTypeName())) return;
            auto* rec = game::Waystones::get().findAt(game::globalPos(region, pos));
            if (!rec) return;
            ev.cancel(); // world edits are deferred: a cancelled interaction rolls them back
            auto key = actorKey(player);
            if (auto it = mThrottle.find(key); it != mThrottle.end() && mNow - it->second < kInteractThrottle) return;
            mThrottle[key] = mNow;
            auto id        = game::idOf(ev.item());
            if (logic::isWarpShard(id)) {
                mPending.push_back({Pending::Kind::Insert, key, rec->uid, player.getSelectedItemSlot()});
            } else if (player.isSneaking() && ev.item().isNull()) {
                mPending.push_back({Pending::Kind::Eject, key, rec->uid, 0});
            } else {
                mPending.push_back({Pending::Kind::Edit, key, rec->uid, 0});
            }
        },
        ll::event::EventPriority::High
    ));

    mListeners.push_back(bus.emplaceListener<ll::event::PlayerPlacingBlockEvent>(
        [this](ll::event::PlayerPlacingBlockEvent& ev) {
            auto& player = ev.self();
            if (player.isClientSide()) return;
            auto const& item = player.getSelectedItem();
            if (!isPlateBlock(game::idOf(item))) return;
            mPlacing[actorKey(player)] = Placing{actorKey(player), game::userString(item, ids::kNbtWaystone), mNow};
        },
        ll::event::EventPriority::Low
    ));
    mListeners.push_back(
        bus.emplaceListener<ll::event::PlayerPlacedBlockEvent>([this](ll::event::PlayerPlacedBlockEvent& ev) {
            auto& player = ev.self();
            if (player.isClientSide() || !isPlateBlock(ev.placedBlock().getTypeName())) return;
            auto&                      region = player.getDimensionBlockSource();
            std::optional<std::string> itemUid;
            if (auto it = mPlacing.find(actorKey(player)); it != mPlacing.end()) {
                itemUid = it->second.uid;
                mPlacing.erase(it);
            }
            onPlaced(&player, game::globalPos(region, ev.pos()), itemUid, true);
        })
    );

    mListeners.push_back(bus.emplaceListener<ll::event::PlayerDestroyBlockEvent>(
        [this](ll::event::PlayerDestroyBlockEvent& ev) {
            auto& player = ev.self();
            if (player.isClientSide() || ev.isCancelled()) return;
            auto& region = player.getDimensionBlockSource();
            if (!isPlateBlock(region.getBlock(ev.pos()).getTypeName())) return;
            auto const& tool = player.getSelectedItem();
            BreakInfo   info;
            info.creative = player.isCreative();
            info.pickaxe  = !tool.isNull() && tool.hasTag(ItemTag{HashedString("minecraft:is_pickaxe")});
            info.silk     = !tool.isNull() && EnchantUtils::hasEnchant(Enchant::Type::SilkTouch, tool);
            noteBreak(game::globalPos(region, ev.pos()), info);
        },
        ll::event::EventPriority::Lowest
    ));

    mListeners.push_back(bus.emplaceListener<ll::event::BlockChangedEvent>([this](ll::event::BlockChangedEvent& ev) {
        if (ev.layer() != 0) return;
        auto& region = ev.blockSource();
        if (region.getLevel().isClientSide()) return;
        bool was = isPlateBlock(ev.previousBlock().getTypeName());
        bool is  = isPlateBlock(ev.newBlock().getTypeName());
        if (was == is) return;
        auto gpos = game::globalPos(region, ev.pos());
        if (is) {
            mOrphans.push_back(gpos); // records are created on placement; others get one next tick
            return;
        }
        auto* rec = game::Waystones::get().findAt(gpos);
        if (!rec || rec->type != core::types::kWarpPlate || rec->transient) return; // transient: already broken
        std::optional<BreakInfo> info;
        if (auto it = mBreaks.find(gpos.key()); it != mBreaks.end()) {
            info = it->second;
            mBreaks.erase(it);
        }
        onRemoved(*rec, info);
    }));
}

void WarpPlates::uninstall() {
    auto& bus = ll::event::EventBus::getInstance();
    for (auto& l : mListeners) bus.removeListener(l);
    mListeners.clear();
    mRt.clear();
    mPending.clear();
    mDrops.clear();
    mOrphans.clear();
    mBreaks.clear();
    mPlacing.clear();
    mThrottle.clear();
}

} // namespace bs::feature::warpplate

BS_REGISTER_MODULE(bs::feature::warpplate::WarpPlates);

// Arriving at a plate (any warp, any ITeleport): the plate can't send the player back until it
// has stepped off for the cooldown time.
BS_REGISTER_WARP_LISTENER([](Player&                         player,
                             bs::service::WarpRequest const& request,
                             bs::service::Destination const& dest) {
    auto  uid = dest.targetUid.empty() ? request.targetId : dest.targetUid;
    auto* rec = bs::game::Waystones::get().find(uid);
    if (rec && rec->type == bs::core::types::kWarpPlate) {
        bs::feature::warpplate::WarpPlates::get().markCooldown(uid, player.getOrCreateUniqueID().rawID);
    }
});
