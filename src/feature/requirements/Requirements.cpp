// U8: the IRequirements seam (Waystones' WarpRequirement system), cooldown storage and sync.

#include "feature/requirements/Requirements.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/server/ServerStoppingEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/string/HashedString.h"
#include "mc/deps/vanilla_components/StrictActorIDEntityContextPair.h"
#include "mc/entity/components/AttributesComponent.h"
#include "mc/legacy/ActorUniqueID.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/provider/ActorRiding.h"
#include "mc/world/attribute/AttributeInstance.h"
#include "mc/world/attribute/AttributeInstanceConstRef.h"
#include "mc/world/attribute/BaseAttributeMap.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/phys/AABB.h"

#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "net/Companion.h"
#include "net/Messages.h"
#include "service/Services.h"

namespace bs::feature::requirements {

namespace req = core::requirements;

BS_REGISTER_CONFIG("requirements", RequirementsConfig);

// ---- cooldowns -------------------------------------------------------------------------------------

std::int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::map<std::string, std::int64_t> activeCooldowns(Player& player) {
    auto& ws   = game::Waystones::get();
    auto& data = ws.playerData(player);
    auto  now  = nowMs();
    std::map<std::string, std::int64_t> out;
    bool pruned = false;
    for (auto it = data.cooldowns.begin(); it != data.cooldowns.end();) {
        if (it->second <= now) {
            it     = data.cooldowns.erase(it);
            pruned = true;
        } else {
            out[it->first] = it->second - now;
            ++it;
        }
    }
    if (pruned) ws.markPlayerDirty(game::Waystones::playerKey(player));
    return out;
}

void syncCooldowns(Player& player) {
    nlohmann::json body = nlohmann::json::object();
    for (auto const& [id, ms] : activeCooldowns(player)) body[id] = ms;
    net::sendMessage(player, kCooldownChannel, std::move(body));
}

void setCooldown(Player& player, std::string const& id, double seconds) {
    auto& ws   = game::Waystones::get();
    auto& data = ws.playerData(player);
    if (seconds <= 0) data.cooldowns.erase(id);
    else data.cooldowns[id] = nowMs() + static_cast<std::int64_t>(std::llround(seconds * 1000.0));
    ws.markPlayerDirty(game::Waystones::playerKey(player));
    syncCooldowns(player);
}

void resetCooldowns(Player& player, std::string const& id) {
    auto& ws   = game::Waystones::get();
    auto& data = ws.playerData(player);
    if (id.empty()) data.cooldowns.clear();
    else data.cooldowns.erase(id);
    ws.markPlayerDirty(game::Waystones::playerKey(player));
    syncCooldowns(player);
}

// ---- experience --------------------------------------------------------------------------------------

int playerLevel(Player& player) {
    auto attr = player.getAttribute(Player::LEVEL());
    return attr.mPtr ? static_cast<int>(attr.mPtr->mCurrentValue) : 0;
}

namespace {
/// Points needed to reach `level` from 0 (Java's closed forms), without int overflow.
long long pointsToReach(long long level) {
    if (level <= 16) return level * level + 6 * level;
    if (level <= 31) return (5 * level * level - 81 * level + 720) / 2;
    return (9 * level * level - 325 * level + 4440) / 2;
}
} // namespace

int totalXp(Player& player) {
    int   level    = playerLevel(player);
    auto  attr     = player.getAttribute(Player::EXPERIENCE());
    float progress = attr.mPtr ? attr.mPtr->mCurrentValue : 0.f;
    int   need     = core::xpForLevel(level);
    // Bedrock keeps the progress as a fraction; round it back to whole points.
    int       points = std::clamp(static_cast<int>(std::lround(progress * static_cast<float>(need))), 0, need - 1);
    long long total  = pointsToReach(level) + points;
    return static_cast<int>(std::min<long long>(total, INT_MAX));
}

void setTotalXp(Player& player, int total) {
    total     = std::max(0, total);
    int level = 0;
    // Walk the levels up (O(level); totals grow quadratically, so this stays short).
    while (level < 24791 && pointsToReach(level + 1) <= total) ++level;
    int points = static_cast<int>(total - pointsToReach(level));
    int cur    = playerLevel(player);
    if (level != cur) player.addLevels(level - cur);
    // LeviLamina's setter goes through the mutable attribute, which syncs the bar to the client.
    if (!player.setXpEarnedAtCurrentLevel(points)) {
        if (auto attrs = player.getEntityContext().tryGetComponent<AttributesComponent>()) {
            auto need = static_cast<float>(core::xpForLevel(level));
            (void)attrs->mAttributes->setCurrentValue(Player::EXPERIENCE(), static_cast<float>(points) / need);
        }
    }
}

// ---- display -------------------------------------------------------------------------------------------

std::vector<std::string> tooltipLines(Player const& player, core::Requirement const& r) {
    std::vector<std::string> out;
    if (r.xpPoints > 0) out.push_back("§a" + trFor(player, "tooltip.waystones.xp_requirement", "Experience Cost: {}", r.xpPoints));
    if (r.levels > 0) out.push_back("§a" + trFor(player, "tooltip.waystones.level_requirement", "Level Cost: {}", r.levels));
    double seconds = r.activeCooldown;
    if (seconds <= 0) {
        for (auto const& c : r.cooldowns) {
            if (!c.soft) seconds = std::max(seconds, c.seconds);
        }
    }
    if (seconds > 0) {
        auto whole = static_cast<long long>(std::ceil(seconds));
        out.push_back("§6" + trFor(player, "tooltip.waystones.cooldown_requirement", "Cooldown: {} seconds", whole));
    }
    for (auto const& item : r.items) {
        out.push_back(trFor(player, "tooltip.waystones.item_requirement", "Item Cost: {} {}", item.count, item.id));
    }
    if (!r.refuse.empty()) {
        out.push_back("§c" + (r.refuse.starts_with("$") ? trFor(player, r.refuse.substr(1), r.refuse.substr(1)) : r.refuse));
    }
    return out;
}

// ---- context -------------------------------------------------------------------------------------------

namespace {

/// U7's `teleports` config is read from its file (it is another unit's struct): Waystones only
/// counts pets when `transportPets` allows them.
struct TeleportsView {
    std::string                     transportPets = "DISABLED";
    std::vector<std::string>        denyList{"minecraft:wither"};
    std::filesystem::file_time_type stamp{};
    bool                            loaded = false;
};

TeleportsView const& teleports() {
    static TeleportsView view;
    static auto          lastCheck = std::chrono::steady_clock::time_point{};
    auto                 now       = std::chrono::steady_clock::now();
    if (view.loaded && now - lastCheck < std::chrono::seconds(1)) return view; // menus resolve many entries at once
    lastCheck = now;
    std::error_code ec;
    auto                 path  = config::configDir() / "teleports.json";
    auto                 stamp = std::filesystem::last_write_time(path, ec);
    if (ec) { // no teleports unit: Waystones' defaults
        view        = TeleportsView{};
        view.loaded = true;
        return view;
    }
    if (view.loaded && view.stamp != std::filesystem::file_time_type{} && stamp == view.stamp) return view;
    view        = TeleportsView{};
    view.stamp  = stamp;
    view.loaded = true;
    std::ifstream in(path);
    auto          j = nlohmann::json::parse(in, nullptr, false);
    if (j.is_object()) {
        if (auto it = j.find("transportPets"); it != j.end() && it->is_string()) view.transportPets = it->get<std::string>();
        if (auto it = j.find("entityDenyList"); it != j.end() && it->is_array()) {
            view.denyList.clear();
            for (auto const& e : *it) {
                if (e.is_string()) view.denyList.push_back(e.get<std::string>());
            }
        }
    }
    std::transform(view.transportPets.begin(), view.transportPets.end(), view.transportPets.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return view;
}

/// Families referenced by `is_on_vehicle(#family)` conditions of the program.
std::vector<std::string> referencedFamilies(req::Program const& p) {
    std::vector<std::string> out;
    for (auto const& rule : p.rules) {
        for (auto const& c : rule.conditions) {
            if (c.name.find("on_vehicle") == std::string::npos || c.args.empty() || !c.args[0].starts_with("#")) continue;
            auto f = c.args[0].substr(1);
            if (auto colon = f.find(':'); colon != std::string::npos) f = f.substr(colon + 1);
            out.push_back(f);
        }
    }
    return out;
}

constexpr SharedTypes::Legacy::ArmorSlot kArmorSlots[] = {
    SharedTypes::Legacy::ArmorSlot::Head,
    SharedTypes::Legacy::ArmorSlot::Torso,
    SharedTypes::Legacy::ArmorSlot::Legs,
    SharedTypes::Legacy::ArmorSlot::Feet,
};

} // namespace

req::Program const& program() {
    static std::vector<std::string> source;
    static req::Program             parsed;
    static bool                     initialized = false;
    auto const&                     rules       = config::get<RequirementsConfig>().warpRequirements;
    if (!initialized || rules != source) {
        source      = rules;
        parsed      = req::parse(rules);
        initialized = true;
        for (auto const& e : parsed.errors) log().warn("requirements: ignoring invalid warp requirement {}", e);
    }
    return parsed;
}

req::Context buildContext(Player& player, core::WarpSource const& source, core::WarpTarget const& target) {
    auto&        ws = game::Waystones::get();
    req::Context c;
    c.playerDim  = static_cast<int>(player.getDimensionId());
    c.sourceKind = source.kind;
    c.sourceDim  = c.playerDim;
    if (auto* from = source.fromUid.empty() ? nullptr : ws.find(source.fromUid)) {
        c.sourceType = from->type;
        c.sourceName = from->name;
        c.sourceDim  = from->pos.dim;
    } else if (source.kind == core::WarpSourceKind::Waystone) {
        c.sourceType = std::string(core::types::kWaystone);
    } else if (source.kind == core::WarpSourceKind::WarpPlate) {
        c.sourceType = std::string(core::types::kWarpPlate);
    } else if (source.kind == core::WarpSourceKind::WarpPortal) {
        c.sourceType = std::string(core::types::kWarpPortal);
    }

    core::GlobalPos pos = target.pos;
    c.targetType        = target.kind;
    c.targetName        = target.name;
    c.targetGlobal      = target.global;
    if (auto* to = target.id.empty() ? nullptr : ws.find(target.id)) {
        c.targetType   = to->type;
        c.targetName   = to->name;
        c.targetGlobal = to->isGlobal();
        pos            = to->pos;
    } else if (target.kind.empty() && !target.id.empty()) {
        // Teleport paths may pass only the id (provider entries such as memorials or players):
        // take the kind, name and flags from the menu entry. Guarded: listing resolves too.
        static bool listing = false;
        if (!listing) {
            listing = true;
            for (auto const& e : service::get<service::ITargets>().list(player, source)) {
                if (e.id != target.id) continue;
                c.targetType   = e.kind;
                c.targetName   = e.name;
                c.targetGlobal = e.global;
                break;
            }
            listing = false;
        }
    }
    c.targetDim = pos.dim;
    Vec3 const& at = player.getPosition();
    double      dx = at.x - (pos.x + 0.5), dy = at.y - pos.y, dz = at.z - (pos.z + 0.5);
    c.distance     = std::sqrt(dx * dx + dy * dy + dz * dz);

    // Companions within 10 blocks (Waystones' WarpContext): leashed mobs and pets.
    auto        self = player.getOrCreateUniqueID();
    auto const& tp   = teleports();
    bool        pets = tp.transportPets == "ENABLED" || (tp.transportPets == "SAME_DIMENSION" && c.playerDim == c.targetDim);
    AABB        box{Vec3{at.x - 10, at.y - 10, at.z - 10}, Vec3{at.x + 10, at.y + 10, at.z + 10}};
    for (auto* a : player.getDimensionBlockSource().getEntities(box, 0.f)) {
        if (!a || a == &player || a->isPlayer()) continue;
        if (a->getPosition().distanceTo(at) > 10.f) continue;
        if (a->isLeashed()) {
            if (a->getLeashHolder() == self) ++c.leashed;
            continue;
        }
        if (!pets || !a->isTame() || a->isSitting() || a->getOwnerId() != self) continue;
        if (std::find(tp.denyList.begin(), tp.denyList.end(), a->getTypeName()) != tp.denyList.end()) continue;
        ++c.pets;
    }
    c.passengers = static_cast<int>(ActorRiding::getPassengers(player.getEntityContext()).size());
    if (auto* vehicle = player.getVehicle()) {
        c.onVehicle   = true;
        c.vehicleType = vehicle->getTypeName();
        for (auto const& f : referencedFamilies(program())) {
            if (vehicle->hasFamily(HashedString(f))) c.vehicleFamilies.push_back(f);
        }
    }

    c.cooldownRemainingMs = activeCooldowns(player);
    auto count            = [&c](ItemStack const& s) {
        if (s.isNull()) return;
        c.emptyInventory = false;
        c.itemCounts[game::idOf(s)] += static_cast<int>(s.mCount);
    };
    int size = player.getInventory().getContainerSize();
    for (int i = 0; i < size; ++i) count(game::itemAt(player, {game::ItemLocation::Kind::Inventory, i}));
    count(game::itemAt(player, {game::ItemLocation::Kind::Offhand, 0}));
    // Armor counts for "empty inventory" but not for has_item (item costs can only take from the
    // inventory and offhand).
    for (auto slot : kArmorSlots) {
        if (player.getArmor(slot).isNull()) continue;
        c.wearingArmor   = true;
        c.emptyInventory = false;
    }
    c.tags = player.getTags();
    return c;
}

// ---- the seam --------------------------------------------------------------------------------------------

namespace {

int countItem(Player& player, std::string const& id) {
    int  n    = 0;
    int  size = player.getInventory().getContainerSize();
    auto add  = [&](ItemStack const& s) {
        if (!s.isNull() && game::idOf(s) == id) n += static_cast<int>(s.mCount);
    };
    for (int i = 0; i < size; ++i) add(game::itemAt(player, {game::ItemLocation::Kind::Inventory, i}));
    add(game::itemAt(player, {game::ItemLocation::Kind::Offhand, 0}));
    return n;
}

void removeItem(Player& player, std::string const& id, int count) {
    int size = player.getInventory().getContainerSize();
    std::vector<game::ItemLocation> locs;
    for (int i = 0; i < size; ++i) locs.push_back({game::ItemLocation::Kind::Inventory, i});
    locs.push_back({game::ItemLocation::Kind::Offhand, 0});
    for (auto loc : locs) {
        if (count <= 0) break;
        auto const& s = game::itemAt(player, loc);
        if (s.isNull() || game::idOf(s) != id) continue;
        int take = std::min(count, static_cast<int>(s.mCount));
        game::consumeAt(player, loc, take, false);
        count -= take;
    }
}

class Requirements final : public service::IRequirements {
public:
    core::Requirement resolve(Player& player, core::WarpSource const& source, core::WarpTarget const& target) override {
        auto const& cfg = config::get<RequirementsConfig>();
        auto        ctx = buildContext(player, source, target);
        auto        r   = req::evaluate(program(), ctx, {cfg.enableCosts, cfg.enableCooldowns}).toRequirement();
        r.activeCooldown = req::activeCooldownSeconds(r, ctx.cooldownRemainingMs);
        return r;
    }

    bool canAfford(Player& player, core::Requirement const& r) override {
        if (player.isCreative()) return true;
        if (!r.refuse.empty() || r.activeCooldown > 0) return false;
        // Re-check cooldowns now (the requirement may have been resolved a while ago).
        if (req::activeCooldownSeconds(r, activeCooldowns(player)) > 0) return false;
        if (r.xpPoints > 0 && totalXp(player) < r.xpPoints) return false;
        if (r.levels > 0 && playerLevel(player) < r.levels) return false;
        for (auto const& item : r.items) {
            if (countItem(player, item.id) < item.count) return false;
        }
        return true;
    }

    void consume(Player& player, core::Requirement const& r) override {
        auto name = player.getRealName();
        if (r.dismount && player.getVehicle()) player.stopRiding(true, false, false, true);
        if (player.isCreative()) {
            // Creative players pay nothing, and warping resets the cooldowns it would start
            // (Waystones: the inventory button resets cooldowns in creative).
            if (!r.cooldowns.empty()) {
                auto& ws   = game::Waystones::get();
                auto& data = ws.playerData(player);
                for (auto const& c : r.cooldowns) data.cooldowns.erase(c.id);
                ws.markPlayerDirty(game::Waystones::playerKey(player));
                syncCooldowns(player);
                log().info("requirements: creative {} reset {} cooldown(s)", name, r.cooldowns.size());
            }
            return;
        }
        if (r.xpPoints > 0) setTotalXp(player, totalXp(player) - r.xpPoints);
        if (r.levels > 0) player.addLevels(-std::min(r.levels, playerLevel(player)));
        for (auto const& item : r.items) removeItem(player, item.id, item.count);
        if (!r.cooldowns.empty()) {
            auto& ws   = game::Waystones::get();
            auto& data = ws.playerData(player);
            auto  now  = nowMs();
            for (auto const& c : r.cooldowns) {
                data.cooldowns[c.id] = now + static_cast<std::int64_t>(std::llround(c.seconds * 1000.0));
                log().info("requirements: cooldown {} started for {} ({}s)", c.id, name, c.seconds);
            }
            ws.markPlayerDirty(game::Waystones::playerKey(player));
            syncCooldowns(player);
        }
        if (r.xpPoints > 0 || r.levels > 0 || !r.items.empty()) {
            log().info(
                "requirements: {} paid {} xp points, {} levels, {} item stack(s) (now {} points)",
                name,
                r.xpPoints,
                r.levels,
                r.items.size(),
                totalXp(player)
            );
        }
    }
};

} // namespace

// ---- module ------------------------------------------------------------------------------------------------

class RequirementsModule {
public:
    void install() {
        service::set<service::IRequirements>(std::make_unique<Requirements>());
        auto& bus = ll::event::EventBus::getInstance();
        // Sync once per player after the companion handshake (it arrives some ticks after join).
        mListeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([this](auto&) {
            if (++mTick % 20 != 0) return;
            auto level = ll::service::getLevel();
            if (!level || !game::Waystones::get().available()) return;
            level->forEachPlayer([this](Player& p) {
                if (p.isSimulatedPlayer() || !net::hasCompanion(p)) return true;
                if (mSynced.insert(game::Waystones::playerKey(p)).second) syncCooldowns(p);
                return true;
            });
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::PlayerDisconnectEvent>([this](ll::event::PlayerDisconnectEvent& ev) {
            mSynced.erase(game::Waystones::playerKey(ev.self()));
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::ServerStoppingEvent>([this](auto&) { mSynced.clear(); }));
        log().info("requirements: {} warp requirement rule(s) active", program().rules.size());
    }

    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
        mSynced.clear();
        service::reset<service::IRequirements>();
    }

private:
    std::vector<ll::event::ListenerPtr> mListeners;
    std::set<std::string>               mSynced;
    std::uint64_t                       mTick = 0;
};

} // namespace bs::feature::requirements

BS_REGISTER_MODULE(bs::feature::requirements::RequirementsModule);

BS_REGISTER_CLIENT_MESSAGE(std::string(bs::feature::requirements::kCooldownRequestChannel), [](Player& player, nlohmann::json const&) {
    bs::feature::requirements::syncCooldowns(player);
});
