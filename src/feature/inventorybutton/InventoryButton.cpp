// Inventory button, server side: config, state computation + sync, the warp request handler,
// and `/waystones inventorybutton` (the closest equivalent for players without the client mod).

#include "feature/inventorybutton/InventoryButton.h"

#include <unordered_map>

#include "ll/api/command/CommandHandle.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/form/ModalForm.h"
#include "ll/api/service/Bedrock.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/DimensionType.h"

#include "game/Effects.h"
#include "game/Waystones.h"
#include "game/WorldDb.h"
#include "mod/Commands.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "net/Companion.h"
#include "net/Messages.h"
#include "service/Services.h"

BS_REGISTER_CONFIG("inventoryButton", bs::feature::inventorybutton::InventoryButtonConfig);

namespace bs::feature::inventorybutton {

using core::inventorybutton::ButtonState;
using core::inventorybutton::Candidate;
using core::inventorybutton::Mode;

// Command parameters need external linkage.
struct InventoryButtonModeParams {
    std::string value;
};

namespace {

std::unordered_map<std::string, std::string> gLastSent; // player uuid -> last state sent (dump)
bool                                         gDirty = false;

core::WarpSource buttonSource() {
    core::WarpSource s;
    s.kind = core::WarpSourceKind::InventoryButton;
    return s;
}

core::WarpTarget targetOf(core::WaystoneRecord const& r, std::string const& name) {
    core::WarpTarget t;
    t.id     = r.uid;
    t.kind   = r.isSharestone() ? "sharestone" : r.type;
    t.name   = name;
    t.pos    = r.pos;
    t.global = r.isGlobal();
    return t;
}

/// Activated waystones of the player, in activation order.
std::vector<Candidate> candidatesOf(Player& player) {
    auto&                  ws   = game::Waystones::get();
    auto const&            data = ws.playerData(player);
    std::vector<Candidate> out;
    for (auto const& uid : data.activated) {
        auto* r = ws.find(uid);
        if (!r || !r->isWaystone()) continue;
        auto alias = data.aliases.find(uid);
        out.push_back({r->uid, r->name, alias != data.aliases.end() ? alias->second : std::string{}, r->pos});
    }
    return out;
}

/// The return portal record of the player (Portal Scroll), if it still exists.
core::WaystoneRecord* returnPortal(Player& player) {
    auto& ws   = game::Waystones::get();
    auto& data = ws.playerData(player);
    auto  it   = data.extra.find("returnPortal");
    if (it == data.extra.end() || !it->is_string()) return nullptr;
    auto* r = ws.find(it->get<std::string>());
    return r && r->type == core::types::kWarpPortal ? r : nullptr;
}

std::string feedback(Player& p, std::string_view key, std::string_view fallback) { return trFor(p, key, fallback); }

} // namespace

ButtonState computeState(Player& player) {
    auto const& cfg = config::get<InventoryButtonConfig>();
    ButtonState s;
    s.mode      = core::inventorybutton::parseMode(cfg.inventoryButton);
    s.x         = cfg.inventoryButtonX;
    s.y         = cfg.inventoryButtonY;
    s.creativeX = cfg.creativeInventoryButtonX;
    s.creativeY = cfg.creativeInventoryButtonY;
    if (s.mode == Mode::None || !game::Waystones::get().available()) return s;

    auto&                           ws   = game::Waystones::get();
    auto const&                     data = ws.playerData(player);
    std::optional<core::WarpTarget> target;
    if (s.mode == Mode::Nearest) {
        if (auto* portal = returnPortal(player)) {
            target = targetOf(*portal, data.effectiveName(portal->uid, portal->name));
        } else {
            auto c   = candidatesOf(player);
            auto pos = player.getPosition();
            if (auto i =
                    core::inventorybutton::nearest(c, static_cast<int>(player.getDimensionId()), pos.x, pos.y, pos.z)) {
                if (auto* r = ws.find(c[*i].uid)) target = targetOf(*r, data.effectiveName(r->uid, r->name));
            }
        }
    } else if (s.mode == Mode::Named) {
        auto c = candidatesOf(player);
        if (auto i = core::inventorybutton::byName(c, core::inventorybutton::trim(cfg.inventoryButton))) {
            if (auto* r = ws.find(c[*i].uid)) target = targetOf(*r, data.effectiveName(r->uid, r->name));
        }
    }

    if (s.mode == Mode::Any) {
        s.hasTarget = !data.activated.empty();
    } else if (target) {
        s.hasTarget  = true;
        s.targetId   = target->id;
        s.targetName = target->name;
    }
    auto& reqs        = service::get<service::IRequirements>();
    s.requirement     = reqs.resolve(player, buttonSource(), target.value_or(core::WarpTarget{}));
    s.canAfford       = reqs.canAfford(player, s.requirement);
    s.cooldownSeconds = core::inventorybutton::cooldownSeconds(s.requirement.activeCooldown);
    return s;
}

void syncState(Player& player, bool force) {
    if (!net::hasCompanion(player)) return;
    auto json = computeState(player).toJson();
    auto dump = json.dump();
    auto key  = game::Waystones::playerKey(player);
    auto it   = gLastSent.find(key);
    if (!force && it != gLastSent.end() && it->second == dump) return;
    gLastSent[key] = std::move(dump);
    net::sendMessage(player, kStateChannel, std::move(json));
}

bool use(Player& player, std::string const& expectedTarget) {
    auto state = computeState(player);
    auto name  = player.getRealName();
    if (state.mode == Mode::None) {
        log().debug("inventory button: {} used it while it is disabled", name);
        return false;
    }
    if (!state.hasTarget && state.mode != Mode::Any) {
        game::actionBar(
            player,
            "§c"
                + feedback(
                    player,
                    "gui.waystones.inventory.no_waystones_activated",
                    "You have not activated any waystones yet."
                )
        );
        log().info("inventory button: {} has no target", name);
        return false;
    }
    if (!expectedTarget.empty() && state.mode != Mode::Any && expectedTarget != state.targetId) {
        // The target changed between the click and "Yes" (moved, new return portal): don't warp
        // somewhere the player did not confirm.
        log().info("inventory button: {} confirmed a target that is no longer current", name);
        syncState(player, true);
        return false;
    }
    bool creative = player.isCreative();
    if (!state.canAfford && !creative) {
        if (auto const& refuse = state.requirement.refuse; !refuse.empty()) {
            auto text = refuse.starts_with('$') ? feedback(player, refuse.substr(1), refuse.substr(1)) : refuse;
            game::actionBar(player, "§4" + text);
        } else if (state.cooldownSeconds > 0) {
            game::actionBar(
                player,
                "§4" + trFor(player, "bs.inventorybutton.cooldown", "Cooldown: {} seconds", state.cooldownSeconds)
            );
        } else {
            game::actionBar(
                player,
                "§4"
                    + feedback(player, "chat.waystones.not_enough_xp", "You do not have enough experience to teleport.")
            );
        }
        log().info("inventory button: {} refused (cooldown {}s)", name, state.cooldownSeconds);
        syncState(player, true);
        return false;
    }

    bool started = false;
    if (state.mode == Mode::Any) {
        log().info("inventory button: {} opens the selection menu", name);
        service::get<service::ISelection>().open(player, buttonSource());
        started = true;
    } else {
        log().info("inventory button: {} warps to {}", name, state.targetName);
        auto res =
            service::get<service::ITeleport>().warp(player, service::WarpRequest{buttonSource(), state.targetId});
        started = res.ok;
        if (res.ok) log().info("inventory button: {} warped to {}", name, state.targetName);
        else log().info("inventory button: {} warp failed: {}", name, res.error);
    }
    // Waystones: using the button in creative resets the player's cooldowns.
    if (creative) {
        auto& ws   = game::Waystones::get();
        auto& data = ws.playerData(player);
        if (!data.cooldowns.empty()) {
            data.cooldowns.clear();
            ws.markPlayerDirty(game::Waystones::playerKey(player));
        }
    }
    syncState(player, true);
    return started;
}

// ---- client requests --------------------------------------------------------------------------------

BS_REGISTER_CLIENT_MESSAGE(kWarpChannel, [](Player& player, nlohmann::json const& data) {
    std::string expected;
    if (data.is_object()) {
        if (auto it = data.find("targetId"); it != data.end() && it->is_string()) expected = it->get<std::string>();
    }
    use(player, expected);
});

// ---- /waystones inventorybutton -------------------------------------------------------------------------

namespace {

void openFallback(Player& player) {
    auto state = computeState(player);
    if (state.mode == Mode::None) {
        game::chat(
            player,
            "§c" + trFor(player, "bs.inventorybutton.disabled", "The inventory button is disabled on this server.")
        );
        return;
    }
    if (state.mode == Mode::Any || !state.hasTarget || (!state.canAfford && !player.isCreative())) {
        use(player); // opens the menu or shows why it can't
        return;
    }
    // Waystones' confirm screen as a modal form.
    auto title = state.mode == Mode::Nearest
                   ? trFor(player, "gui.waystones.inventory.return_to_nearest_waystone", "Return to Nearest Waystone")
                   : trFor(player, "gui.waystones.inventory.return_to_waystone", "Return to Waystone");
    auto content =
        trFor(player, "gui.waystones.inventory.confirm_return", "Do you really want to warp back to this waystone?")
        + "\n\n§7"
        + trFor(
            player,
            "gui.waystones.inventory.confirm_return_bound_to",
            "Bound to: {}",
            "§3" + state.targetName + "§7"
        );
    ll::form::ModalForm form(
        title,
        content,
        trFor(player, "bs.inventorybutton.yes", "Yes"),
        trFor(player, "bs.inventorybutton.no", "No")
    );
    form.sendTo(player, [](Player& p, ll::form::ModalFormResult result, ll::form::FormCancelReason) {
        if (result && *result == ll::form::ModalFormSelectedButton::Upper) use(p);
    });
}

} // namespace

BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload().text("inventorybutton").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = commands::playerOf(origin);
        if (!player) return output.error("Only players can use the inventory button.");
        openFallback(*player);
        output.success("");
    });
    // Debug / admin: changes the runtime value of `inventoryButton` (not saved to the file).
    cmd.overload<InventoryButtonModeParams>()
        .text("inventorybutton")
        .text("mode")
        .required("value")
        .execute([](CommandOrigin const& origin, CommandOutput& output, InventoryButtonModeParams const& params) {
            if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
            config::mut<InventoryButtonConfig>().inventoryButton = params.value;
            gDirty                                               = true;
            log().info("inventory button mode set to '{}'", params.value);
            output.success("inventoryButton = " + params.value);
        });
});

// ---- module -------------------------------------------------------------------------------------------

class InventoryButtonModule {
public:
    void install() {
        auto& bus = ll::event::EventBus::getInstance();
        mListeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([this](auto&) { tick(); }));
        mListeners.push_back(bus.emplaceListener<ll::event::PlayerDisconnectEvent>(
            [](ll::event::PlayerDisconnectEvent& ev) { gLastSent.erase(game::Waystones::playerKey(ev.self())); }
        ));
        static bool registered = false; // the registries below have no removal
        if (!registered) {
            registered = true;
            config::onReload([] { gDirty = true; });
            game::db::onWorldClosed([] { gLastSent.clear(); });
        }
        auto& ws        = game::Waystones::get();
        mRecordListener = ws.onChanged([](game::WaystoneChange, core::WaystoneRecord const&) { gDirty = true; });
        mPlayerListener =
            ws.onPlayerChanged([](game::PlayerChange, std::string const&, std::string const&) { gDirty = true; });
    }
    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
        auto& ws = game::Waystones::get();
        ws.removeListener(mRecordListener);
        ws.removeListener(mPlayerListener);
        gLastSent.clear();
    }

private:
    void tick() {
        // Every second (positions change the nearest target, cooldowns count down), or on the next
        // tick after a waystone / player data / config change.
        if (++mTicks < 20 && !gDirty) return;
        mTicks = 0;
        gDirty = false;
        if (net::companionCount() == 0 || !game::Waystones::get().available()) return;
        auto level = ll::service::getLevel();
        if (!level) return;
        level->forEachPlayer([](Player& p) {
            if (net::hasCompanion(p)) syncState(p);
            return true;
        });
    }

    std::vector<ll::event::ListenerPtr> mListeners;
    int                                 mRecordListener = -1;
    int                                 mPlayerListener = -1;
    int                                 mTicks          = 0;
};

} // namespace bs::feature::inventorybutton

BS_REGISTER_MODULE(bs::feature::inventorybutton::InventoryButtonModule);
