// ITargets: which entries a selection menu lists for each warp source (docs/SPEC.md §3.7).

#include <algorithm>
#include <optional>
#include <set>

#include "ll/api/service/Bedrock.h"
#include "ll/api/service/PlayerInfo.h"

#include "mc/platform/UUID.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"

#include "core/Waystone.h"
#include "feature/selection/Selection.h"
#include "game/BlockContainer.h"
#include "game/Ids.h"
#include "game/Waystones.h"
#include "service/Services.h"
#include "service/TargetProviders.h"

namespace bs::feature::selection {

namespace {

using core::WarpSourceKind;
using core::WarpTarget;
using core::WaystoneRecord;

std::string kindOf(WaystoneRecord const& r) { return r.isSharestone() ? "sharestone" : r.type; }

WarpTarget fromRecord(core::PlayerWaystoneData const& data, WaystoneRecord const& r, core::WarpSource const& source) {
    WarpTarget t;
    t.id      = r.uid;
    t.kind    = kindOf(r);
    t.name    = data.effectiveName(r.uid, r.name);
    t.pos     = r.pos;
    t.global  = r.isGlobal();
    t.current = !source.fromUid.empty() && r.uid == source.fromUid;
    return t;
}

/// The sharestone colour a source lists (sharestones and coloured portstones), nullopt for the
/// waystone list.
std::optional<std::string> sharestoneColorOf(core::WarpSource const& source) {
    if (source.kind == WarpSourceKind::Sharestone) {
        if (auto* from = game::Waystones::get().find(source.fromUid)) {
            if (auto c = core::types::sharestoneColor(from->type)) return c;
        }
        if (!source.portstoneColor.empty() && source.portstoneColor != "white") return source.portstoneColor;
        return std::nullopt;
    }
    if (source.kind == WarpSourceKind::Portstone && !source.portstoneColor.empty() && source.portstoneColor != "white") {
        return source.portstoneColor;
    }
    return std::nullopt;
}

/// Sources whose block container contributes the shards / bound scrolls it holds.
bool listsContainer(core::WarpSource const& source) {
    return source.kind == WarpSourceKind::Waystone || source.kind == WarpSourceKind::Sharestone;
}

} // namespace

std::vector<WarpTarget> computeTargets(Player& player, core::WarpSource const& source) {
    auto& ws    = game::Waystones::get();
    auto& data  = ws.playerData(player);
    auto& perms = service::get<service::IPermissions>();
    auto  color = sharestoneColorOf(source);

    std::vector<WarpTarget> out;
    std::set<std::string>   seen;
    auto add = [&](WarpTarget t) {
        if (!seen.insert(t.id).second) return;
        out.push_back(std::move(t));
    };

    for (auto* r : ws.all()) {
        bool isSource = !source.fromUid.empty() && r->uid == source.fromUid;
        bool listed   = false;
        if (color) {
            listed = r->type == core::types::sharestone(*color) && (!r->transient || isSource);
        } else if (r->isWaystone()) {
            bool shared = (r->isGlobal() || r->visibility == core::Visibility::Team) && !r->transient && perms.canSee(player, *r);
            listed      = data.isActivated(r->uid) || shared || isSource;
        }
        if (!listed) continue;
        if (!isSource && data.hidden.contains(r->uid)) continue; // hidden from the menus (Manage screen eye)
        add(fromRecord(data, *r, source));
    }

    // Attuned shards / bound scrolls kept in the source block's container.
    if (listsContainer(source)) {
        if (auto* from = ws.find(source.fromUid)) {
            for (auto const& slot : game::containerOf(*from)) {
                if (!slot) continue;
                for (auto key : {ids::kNbtAttunement, ids::kNbtBoundUid}) {
                    auto it = slot->user.find(std::string(key));
                    if (it == slot->user.end() || it->second.empty()) continue;
                    if (auto* r = ws.find(it->second); r && !r->transient) add(fromRecord(data, *r, source));
                }
            }
        }
    }

    // Twinbound players, memorials, the return portal, ...: the providers decide per source.
    for (auto const& p : service::TargetProviderRegistry::entries()) {
        if (!p.list) continue;
        std::vector<WarpTarget> extra;
        p.list(player, source, extra);
        for (auto& t : extra) add(std::move(t));
    }

    auto& reqs   = service::get<service::IRequirements>();
    auto& groups = service::get<service::IGroups>();
    for (auto& t : out) {
        if (t.kind == core::types::kWarpPortal || t.kind == core::types::kFleetingMemorial) t.pinned = true;
        t.requirement = reqs.resolve(player, source, t);
        t.canAfford   = reqs.canAfford(player, t.requirement);
        t.groups      = groups.groupsOf(player, t);
    }
    return out;
}

std::vector<WarpTarget> adminTargets(Player& viewer, std::string const& ownerUuid) {
    auto& ws     = game::Waystones::get();
    auto& owner  = ws.playerData(ownerUuid);
    auto& groups = service::get<service::IGroups>();
    core::WarpSource none;
    none.kind = WarpSourceKind::Admin;
    std::vector<WarpTarget> out;
    for (auto const& uid : owner.activated) {
        auto* r = ws.find(uid);
        if (!r) continue;
        auto t      = fromRecord(owner, *r, none);
        t.canAfford = true; // admin warps skip requirements
        t.groups    = groups.groupsOf(viewer, t);
        out.push_back(std::move(t));
    }
    return out;
}

std::string playerName(std::string const& uuid) {
    if (!mce::UUID::canParse(uuid)) return uuid;
    mce::UUID id = mce::UUID::fromString(uuid);
    if (auto level = ll::service::getLevel()) {
        if (auto* p = level->getPlayer(id)) return p->getRealName();
    }
    if (auto info = ll::service::PlayerInfo::getInstance().fromUuid(id)) return info->name;
    return uuid;
}

} // namespace bs::feature::selection
