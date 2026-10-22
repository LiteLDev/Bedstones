// Minimal default implementations of the service seams (see Services.h). Feature units replace
// them with the faithful Waystones behaviour; these only keep the mod usable on its own.

#include <algorithm>
#include <cmath>

#include "ll/api/form/CustomForm.h"
#include "ll/api/form/SimpleForm.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/DimensionType.h"

#include "core/Waystone.h"
#include "game/Blocks.h"
#include "game/Effects.h"
#include "game/Waystones.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "service/Services.h"
#include "service/TargetProviders.h"

namespace bs::service {

namespace {

using core::WaystoneRecord;

std::string typeLabel(Player const& p, WaystoneRecord const& r) {
    if (r.isSharestone()) return trFor(p, "bs.foundation.type.sharestone", "Sharestone");
    if (r.type == core::types::kWarpPlate) return trFor(p, "bs.foundation.type.warp_plate", "Warp Plate");
    return trFor(p, "bs.foundation.type.waystone", "Waystone");
}

// ---- teleport ---------------------------------------------------------------------------------------

class DefaultTeleport final : public ITeleport {
public:
    std::optional<Destination> resolve(Player& player, std::string const& targetId) override {
        if (auto const* provider = providerFor(targetId)) {
            return provider->resolve ? provider->resolve(player, targetId) : std::nullopt;
        }
        auto* r = game::Waystones::get().find(targetId);
        if (!r) return std::nullopt;
        auto [dx, dz] = game::facingOffset(r->facing);
        Destination d;
        d.block     = r->pos;
        d.x         = static_cast<float>(r->pos.x + dx) + 0.5f;
        d.y         = static_cast<float>(r->pos.y);
        d.z         = static_cast<float>(r->pos.z + dz) + 0.5f;
        d.yaw       = static_cast<float>(r->facing) * 90.f;
        d.targetUid = r->uid;
        return d;
    }

    WarpResult warp(Player& player, WarpRequest const& request) override {
        auto dest = resolve(player, request.targetId);
        if (!dest) {
            auto res = WarpResult::fail(
                "chat.waystones.waystone_missing",
                "The waystone you tried to teleport to is currently being moved or has gone missing."
            );
            game::actionBar(player, "§4" + trFor(player, res.error, res.errorFallback));
            return res;
        }
        if (!request.skipRequirements) {
            auto& reqs = get<IRequirements>();
            core::WarpTarget target;
            target.id   = request.targetId;
            target.pos  = dest->block;
            auto req    = reqs.resolve(player, request.source, target);
            if (!reqs.canAfford(player, req)) {
                auto res = WarpResult::fail("chat.waystones.not_enough_xp", "You do not have enough experience to teleport.");
                game::actionBar(player, "§4" + trFor(player, res.error, res.errorFallback));
                return res;
            }
            reqs.consume(player, req);
        }
        Vec3 from = player.getPosition();
        int  fromDim = static_cast<int>(player.getDimensionId());
        player.teleport(Vec3{dest->x, dest->y, dest->z}, DimensionType(dest->block.dim), Vec2{0.f, dest->yaw});
        game::soundAround(fromDim, "mob.endermen.portal", from, 0.1f, 1.f);
        game::soundAround(dest->block.dim, "mob.endermen.portal", Vec3{dest->x, dest->y, dest->z}, 0.5f, 1.f);
        if (auto const* provider = providerFor(request.targetId); provider && provider->onWarped) {
            provider->onWarped(player, request.targetId);
        }
        if (auto* r = game::Waystones::get().find(dest->targetUid)) get<IArrival>().onArrived(player, *r);
        log().info("warp: {} -> {}", player.getRealName(), request.targetId);
        return WarpResult::success();
    }
};

// ---- requirements -------------------------------------------------------------------------------------

class DefaultRequirements final : public IRequirements {
public:
    core::Requirement resolve(Player&, core::WarpSource const&, core::WarpTarget const&) override { return {}; }
    bool              canAfford(Player& player, core::Requirement const& r) override {
        return player.isCreative() || (r.refuse.empty() && r.activeCooldown <= 0);
    }
    void consume(Player&, core::Requirement const&) override {}
};

// ---- permissions --------------------------------------------------------------------------------------

class DefaultPermissions final : public IPermissions {
public:
    std::optional<Denied> canEdit(Player const& player, WaystoneRecord const& r) override {
        if (player.isCreative()) return std::nullopt;
        if (r.isGlobal()) return Denied{"chat.waystones.only_creative_can_edit_global", "Only creative users may edit this waystone."};
        if (r.hasOwner() && r.ownerUid != game::Waystones::playerKey(player)) {
            return Denied{"chat.waystones.only_owner_can_edit", "Only the owner may edit this waystone."};
        }
        return std::nullopt;
    }
    bool canManageGlobal(Player const& player) override { return player.isCreative(); }
    std::vector<core::Visibility> visibilityOptions(Player const& player, WaystoneRecord const& r) override {
        if (r.isSharestone()) return {core::Visibility::Sharestone};
        if (r.type == core::types::kWarpPlate) return {core::Visibility::ShardOnly};
        std::vector<core::Visibility> v{core::Visibility::Activation};
        if (canManageGlobal(player)) v.push_back(core::Visibility::Global);
        return v;
    }
    core::Visibility defaultVisibility() override { return core::Visibility::Activation; }
    bool             canSee(Player const& player, WaystoneRecord const& r) override {
        switch (r.visibility) {
        case core::Visibility::Global:
        case core::Visibility::Sharestone:
            return !r.transient;
        case core::Visibility::ShardOnly:
            return false;
        case core::Visibility::Team:
        case core::Visibility::Activation:
            return game::Waystones::get().isActivated(player, r.uid) || r.ownerUid == game::Waystones::playerKey(player);
        }
        return false;
    }
};

// ---- names ----------------------------------------------------------------------------------------------

class DefaultNames final : public INameGenerator {
public:
    std::string generate(core::Origin, std::string const&) override {
        auto n = game::Waystones::get().all().size() + 1;
        return "Waystone " + std::to_string(n);
    }
};

// ---- targets ----------------------------------------------------------------------------------------------

class DefaultTargets final : public ITargets {
public:
    std::vector<core::WarpTarget> list(Player& player, core::WarpSource const& source) override {
        auto& ws    = game::Waystones::get();
        auto& data  = ws.playerData(player);
        auto& perms = get<IPermissions>();
        auto& reqs  = get<IRequirements>();
        std::optional<std::string> color;
        if (source.kind == core::WarpSourceKind::Sharestone) {
            if (auto* from = ws.find(source.fromUid)) color = core::types::sharestoneColor(from->type);
        } else if (source.kind == core::WarpSourceKind::Portstone && source.portstoneColor != "white") {
            color = source.portstoneColor;
        }
        std::vector<core::WarpTarget> out;
        for (auto* r : ws.all()) {
            bool listed = color ? r->type == core::types::sharestone(*color)
                                : r->isWaystone() && !r->transient && (data.isActivated(r->uid) || (r->isGlobal() && perms.canSee(player, *r)));
            if (!listed) continue;
            core::WarpTarget t;
            t.id        = r->uid;
            t.kind      = r->isSharestone() ? "sharestone" : std::string(r->type);
            t.name      = data.effectiveName(r->uid, r->name);
            t.pos       = r->pos;
            t.global    = r->isGlobal();
            t.current   = r->uid == source.fromUid;
            t.requirement = reqs.resolve(player, source, t);
            t.canAfford = reqs.canAfford(player, t.requirement);
            out.push_back(std::move(t));
        }
        for (auto const& p : TargetProviderRegistry::entries()) {
            if (p.list) p.list(player, source, out);
        }
        return out;
    }
};

// ---- selection (form) -----------------------------------------------------------------------------------

class DefaultSelection final : public ISelection {
public:
    void open(Player& player, core::WarpSource const& source) override {
        auto targets = get<ITargets>().list(player, source);
        std::erase_if(targets, [](auto const& t) { return t.current; });
        ll::form::SimpleForm form(trFor(player, "gui.waystones.waystone_selection.title", "Select your destination"));
        if (targets.empty()) form.setContent(trFor(player, "gui.waystones.waystone_selection.no_waystones_activated", "You have not activated any waystones yet."));
        for (auto const& t : targets) form.appendButton(t.name.empty() ? "<unnamed>" : t.name);
        form.sendTo(player, [targets, source](Player& p, int index, ll::form::FormCancelReason) {
            if (index < 0 || index >= static_cast<int>(targets.size())) return;
            get<ITeleport>().warp(p, WarpRequest{source, targets[static_cast<std::size_t>(index)].id});
        });
    }
    void openAdmin(Player& viewer, std::string const& ownerUuid) override {
        auto& ws = game::Waystones::get();
        auto& d  = ws.playerData(ownerUuid);
        std::vector<std::string> uids = d.activated;
        ll::form::SimpleForm form(trFor(viewer, "gui.waystones.waystone_selection.title", "Select your destination"));
        for (auto const& uid : uids) {
            auto* r = ws.find(uid);
            form.appendButton(r ? r->name : uid);
        }
        form.sendTo(viewer, [uids](Player& p, int index, ll::form::FormCancelReason) {
            if (index < 0 || index >= static_cast<int>(uids.size())) return;
            core::WarpSource src;
            src.kind = core::WarpSourceKind::Admin;
            get<ITeleport>().warp(p, WarpRequest{src, uids[static_cast<std::size_t>(index)], true});
        });
    }
};

// ---- edit (form) ------------------------------------------------------------------------------------------

class DefaultEdit final : public IEditScreen {
public:
    void open(Player& player, std::string const& uid) override {
        auto* r = game::Waystones::get().find(uid);
        if (!r) return;
        auto& perms   = get<IPermissions>();
        auto  denied  = perms.canEdit(player, *r);
        auto  options = perms.visibilityOptions(player, *r);
        std::vector<std::string> labels;
        std::size_t              current = 0;
        for (std::size_t i = 0; i < options.size(); ++i) {
            labels.emplace_back(core::toString(options[i]));
            if (options[i] == r->visibility) current = i;
        }
        ll::form::CustomForm form(trFor(player, "gui.waystones.waystone_settings.title", "Edit {}", typeLabel(player, *r)));
        if (denied) form.appendLabel("§c" + trFor(player, denied->key, denied->fallback));
        form.appendInput("name", trFor(player, "bs.foundation.name", "Name"), "Untitled Waystone", r->name);
        form.appendDropdown("visibility", trFor(player, "bs.foundation.visibility", "Visibility"), labels, current);
        form.sendTo(player, [uid, options, editable = !denied](Player& p, ll::form::CustomFormResult const& res, ll::form::FormCancelReason) {
            if (!res || !editable) return;
            auto* rec = game::Waystones::get().find(uid);
            if (!rec) return;
            if (auto it = res->find("name"); it != res->end()) {
                if (auto const* s = std::get_if<std::string>(&it->second)) rec->name = s->empty() ? "Untitled Waystone" : *s;
            }
            if (auto it = res->find("visibility"); it != res->end()) {
                if (auto const* idx = std::get_if<uint64>(&it->second); idx && *idx < options.size()) rec->visibility = options[*idx];
            }
            game::Waystones::get().markDirty(uid);
            (void)p;
        });
    }
};

class DefaultArrival final : public IArrival {
public:
    void onArrived(Actor&, WaystoneRecord const&) override {}
};

} // namespace

TargetProvider const* providerFor(std::string const& targetId) {
    for (auto const& p : TargetProviderRegistry::entries()) {
        if (p.owns && p.owns(targetId)) return &p;
    }
    return nullptr;
}

namespace detail {
template <>
std::unique_ptr<ITeleport> makeDefault<ITeleport>() {
    return std::make_unique<DefaultTeleport>();
}
template <>
std::unique_ptr<IRequirements> makeDefault<IRequirements>() {
    return std::make_unique<DefaultRequirements>();
}
template <>
std::unique_ptr<IPermissions> makeDefault<IPermissions>() {
    return std::make_unique<DefaultPermissions>();
}
template <>
std::unique_ptr<INameGenerator> makeDefault<INameGenerator>() {
    return std::make_unique<DefaultNames>();
}
template <>
std::unique_ptr<ITargets> makeDefault<ITargets>() {
    return std::make_unique<DefaultTargets>();
}
template <>
std::unique_ptr<ISelection> makeDefault<ISelection>() {
    return std::make_unique<DefaultSelection>();
}
template <>
std::unique_ptr<IEditScreen> makeDefault<IEditScreen>() {
    return std::make_unique<DefaultEdit>();
}
template <>
std::unique_ptr<IArrival> makeDefault<IArrival>() {
    return std::make_unique<DefaultArrival>();
}
} // namespace detail

} // namespace bs::service
