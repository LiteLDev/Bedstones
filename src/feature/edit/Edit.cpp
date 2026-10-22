// Edit Waystone screen, server side (docs/SPEC.md §3.4, §4.5).

#include "feature/edit/Edit.h"

#include <cmath>
#include <filesystem>
#include <fstream>

#include "ll/api/command/CommandHandle.h"
#include "ll/api/form/CustomForm.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"

#include "core/modifiers/EditRules.h"
#include "feature/modifiers/Modifiers.h"
#include "game/Blocks.h"
#include "game/Waystones.h"
#include "mod/Commands.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "net/Screens.h"
#include "service/ScreenOpeners.h"
#include "service/Services.h"

namespace bs::feature::edit {

namespace {

constexpr char const* kScreen = "edit";

/// "waystone", "sharestone", "warp_plate" or the record type (title and icon of the screen).
std::string kindOf(core::WaystoneRecord const& r) {
    if (r.isSharestone()) return "sharestone";
    if (r.type == core::types::kWarpPlate) return "warp_plate";
    return "waystone";
}

using modifiers::hasContainer;
using modifiers::withinReach;

/// The `inventoryButton` setting (U14's config section, read from its file when present).
std::string inventoryButtonMode() {
    std::error_code ec;
    auto            path = config::configDir() / "inventoryButton.json";
    if (!std::filesystem::exists(path, ec)) return {};
    std::ifstream in(path);
    auto          j = nlohmann::json::parse(in, nullptr, false);
    if (!j.is_object()) return {};
    auto it = j.find("inventoryButton");
    return it != j.end() && it->is_string() ? it->get<std::string>() : std::string{};
}

std::string visibilityLabel(Player const& p, core::Visibility v, core::WaystoneRecord const& r) {
    switch (v) {
    case core::Visibility::Activation:
        return trFor(p, "gui.waystones.waystone_settings.visibility.activation", "Visible upon Activation");
    case core::Visibility::Global:
        return trFor(p, "gui.waystones.waystone_settings.visibility.global", "Visible to Everyone");
    case core::Visibility::Team:
        return trFor(p, "gui.waystones.waystone_settings.visibility.team", "Visible to Team");
    case core::Visibility::ShardOnly:
        return trFor(p, "gui.waystones.waystone_settings.visibility.shard_only", "Teleports via Attuned Shard");
    case core::Visibility::Sharestone: {
        auto color = core::types::sharestoneColor(r.type).value_or("white");
        auto name  = trFor(p, "bs.edit.color." + color, color);
        return trFor(p, "gui.waystones.waystone_settings.visibility.sharestone", "Visible to {} Sharestones", name);
    }
    }
    return {};
}

std::string typeLabel(Player const& p, std::string const& kind) {
    if (kind == "sharestone") return trFor(p, "bs.foundation.type.sharestone", "Sharestone");
    if (kind == "warp_plate") return trFor(p, "bs.foundation.type.warp_plate", "Warp Plate");
    return trFor(p, "bs.foundation.type.waystone", "Waystone");
}

nlohmann::json defaultSource(std::string const& uid) {
    core::WarpSource src;
    src.kind    = core::WarpSourceKind::Waystone;
    src.fromUid = uid;
    if (auto* r = game::Waystones::get().find(uid)) src.fromPos = r->pos;
    return src.toJson();
}

// ---- actions ----------------------------------------------------------------------------------------------

void handleAction(Player& player, net::screens::ScreenSession& s, std::string const& action, nlohmann::json const& args) {
    if (action == "close") return; // closed without saving (the game dropped the screen)
    auto uid        = s.context.value("uid", std::string{});
    auto name       = args.value("name", std::string{});
    auto visibility = args.value("visibility", std::string{});
    if (action == "save") {
        save(player, uid, name, visibility);
        net::screens::close(player);
        return;
    }
    if (action == "personal_settings" || action == "modifiers") {
        // Leaving the screen saves it like closing does (only when editing is allowed).
        auto* r = game::Waystones::get().find(uid);
        if (!r) return net::screens::close(player);
        if (!service::get<service::IPermissions>().canEdit(player, *r)) save(player, uid, name, visibility);
        auto session = s.session; // `s` is replaced when another screen opens
        if (action == "modifiers") {
            if (modifiers::enabled() && hasContainer(*r)) service::openScreen(player, "modifiers", {{"uid", uid}});
        } else {
            auto source = s.context.value("source", nlohmann::json{});
            if (source.is_null()) source = defaultSource(uid);
            service::openScreen(player, "personal_settings", {{"uid", uid}, {"source", source}});
        }
        // The other screen replaced this one; when it refused to open, close.
        if (net::screens::current(player, kScreen, session)) net::screens::close(player);
        return;
    }
}

// ---- form fallback --------------------------------------------------------------------------------------

void showFallback(Player& player, net::screens::ScreenSession& s) {
    auto const& m       = s.model;
    auto*       r       = game::Waystones::get().find(m.value("uid", std::string{}));
    if (!r) return;
    auto        options = m.value("options", std::vector<std::string>{});
    std::vector<std::string> labels;
    std::size_t              current = 0;
    for (std::size_t i = 0; i < options.size(); ++i) {
        auto v = core::visibilityFromString(options[i]).value_or(core::Visibility::Activation);
        labels.push_back(visibilityLabel(player, v, *r));
        if (options[i] == m.value("visibility", std::string{})) current = i;
    }
    ll::form::CustomForm form(trFor(player, "gui.waystones.waystone_settings.title", "Edit {}", typeLabel(player, m.value("kind", std::string{}))));
    if (auto const& err = m.value("error", nlohmann::json{}); err.is_object()) {
        form.appendLabel("§c" + trFor(player, err.value("key", std::string{}), err.value("fallback", std::string{})));
    }
    form.appendInput(
        "name",
        trFor(player, "bs.foundation.name", "Name"),
        trFor(player, "gui.waystones.waystone_settings.untitled", "Untitled Waystone"),
        m.value("name", std::string{})
    );
    form.appendDropdown("visibility", trFor(player, "bs.foundation.visibility", "Visibility"), labels, current);
    bool mods = m.value("modifiersAvailable", false) && m.value("canEdit", false);
    if (mods) {
        int  n     = m.value("modifierCount", 0);
        auto count = n > 0 ? trFor(player, "gui.waystones.waystone_settings.modifiers_active", "{} modifiers active", n)
                           : trFor(player, "gui.waystones.waystone_settings.no_modifiers_active", "No modifiers active");
        form.appendToggle("modifiers", trFor(player, "bs.edit.form.manage_modifiers", "Manage modifiers ({})", count), false);
    }
    form.sendTo(player, [options](Player& p, ll::form::CustomFormResult const& res, ll::form::FormCancelReason) {
        if (!res) return net::screens::simulateAction(p, "close");
        nlohmann::json args{{"name", ""}, {"visibility", ""}};
        if (auto it = res->find("name"); it != res->end()) {
            if (auto const* str = std::get_if<std::string>(&it->second)) args["name"] = *str;
        }
        if (auto it = res->find("visibility"); it != res->end()) {
            if (auto const* idx = std::get_if<uint64>(&it->second); idx && *idx < options.size()) args["visibility"] = options[*idx];
        }
        bool manage = false;
        if (auto it = res->find("modifiers"); it != res->end()) {
            if (auto const* b = std::get_if<uint64>(&it->second)) manage = *b != 0;
            else if (auto const* d = std::get_if<double>(&it->second)) manage = *d != 0;
        }
        net::screens::simulateAction(p, manage ? "modifiers" : "save", args);
    });
}

// ---- the seam --------------------------------------------------------------------------------------------

class EditScreen final : public service::IEditScreen {
public:
    void open(Player& player, std::string const& uid) override { openEdit(player, uid); }
};

class EditModule {
public:
    void install() { service::set<service::IEditScreen>(std::make_unique<EditScreen>()); }
    void uninstall() { service::reset<service::IEditScreen>(); }
};

} // namespace

nlohmann::json buildModel(Player& player, std::string const& uid) {
    auto* r = game::Waystones::get().find(uid);
    if (!r) return nullptr;
    auto& perms   = service::get<service::IPermissions>();
    auto  denied  = perms.canEdit(player, *r);
    auto  options = perms.visibilityOptions(player, *r);
    std::vector<std::string> opts;
    for (auto v : options) opts.emplace_back(core::toString(v));
    nlohmann::json model{
        {"uid", r->uid},
        {"type", r->type},
        {"kind", kindOf(*r)},
        {"sharestoneColor", core::types::sharestoneColor(r->type).value_or("")},
        {"name", r->name},
        {"canEdit", !denied},
        {"error", nullptr},
        {"visibility", std::string(core::toString(r->visibility))},
        {"options", opts},
        {"modifierCount", modifiers::activeModifierCount(*r)},
        {"modifiersAvailable", hasContainer(*r) && modifiers::enabled() && service::hasScreen("modifiers")},
        {"personalSettings", service::hasScreen("personal_settings")},
    };
    if (denied) model["error"] = {{"key", denied->key}, {"fallback", denied->fallback}};
    return model;
}

bool openEdit(Player& player, std::string const& uid, nlohmann::json const& source) {
    auto model = buildModel(player, uid);
    if (model.is_null()) return false;
    nlohmann::json context{{"uid", uid}, {"source", source}};
    if (!net::screens::open(player, kScreen, std::move(model), std::move(context))) return false;
    log().info("edit: screen of {} opened by {}", uid, player.getRealName());
    return true;
}

SaveResult save(Player& player, std::string const& uid, std::string const& name, std::string const& visibility) {
    auto& ws = game::Waystones::get();
    auto* r  = ws.find(uid);
    if (!r) return SaveResult::NotFound;
    auto& perms = service::get<service::IPermissions>();
    if (perms.canEdit(player, *r)) {
        log().info("edit: {} may not edit {}; save ignored", player.getRealName(), r->name);
        return SaveResult::Denied;
    }
    if (!withinReach(player, *r, core::edit::kMaxEditDistance)) {
        log().info("edit: {} is too far away from {}; save ignored", player.getRealName(), r->name);
        return SaveResult::TooFar;
    }
    auto finalName = core::edit::finalName(name, inventoryButtonMode(), [&](std::string const& n) {
        for (auto* o : ws.all()) {
            if (o->uid != uid && o->name == n) return true;
        }
        return false;
    });
    auto vis = core::edit::pickVisibility(visibility, perms.visibilityOptions(player, *r));
    r        = ws.find(uid); // the callbacks above may not keep pointers stable; look it up again
    if (!r) return SaveResult::NotFound;
    r->name = finalName;
    if (vis) r->visibility = *vis;
    ws.markDirty(uid);
    log().info("edit: {} saved {} ({}) by {}", r->type, r->name, core::toString(r->visibility), player.getRealName());
    return SaveResult::Saved;
}

} // namespace bs::feature::edit

BS_REGISTER_MODULE(bs::feature::edit::EditModule);

BS_REGISTER_SCREEN_OPENER("edit", [](Player& player, nlohmann::json const& args) {
    bs::feature::edit::openEdit(player, args.value("uid", std::string{}), args.value("source", nlohmann::json{}));
});

BS_REGISTER_SCREEN_ACTIONS("edit", bs::feature::edit::handleAction);
BS_REGISTER_SCREEN_FALLBACK("edit", bs::feature::edit::showFallback);

// ---- `/waystones debug edit`: a waystone next to the player, then its edit screen (e2e) ----------------

BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload().text("debug").text("edit").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = bs::commands::playerOf(origin);
        if (!player) return output.error("Only players can open screens.");
        auto& region = player->getDimensionBlockSource();
        int   facing = bs::game::playerFacing(*player);
        auto [dx, dz] = bs::game::facingOffset(facing);
        auto feet     = player->getFeetPos();
        BlockPos pos{
            static_cast<int>(std::floor(feet.x)) + 2 * dx,
            static_cast<int>(std::floor(feet.y)),
            static_cast<int>(std::floor(feet.z)) + 2 * dz,
        };
        // Run again at the same spot: reuse that waystone.
        if (auto* existing = bs::game::Waystones::get().findAt(bs::game::globalPos(region, pos))) {
            auto uid = existing->uid;
            bs::service::get<bs::service::IEditScreen>().open(*player, uid);
            return output.success("debug waystone " + uid);
        }
        if (!bs::game::placeTwoTall(region, pos, "bedstones:waystone", bs::game::opposite(facing))) {
            // Without the waystone block (blocks unit absent) a stand-in block marks the spot.
            if (auto const* b = bs::game::blockWith("minecraft:lodestone")) bs::game::setBlock(region, pos, *b);
        }
        bs::core::WaystoneRecord rec;
        rec.pos        = bs::game::globalPos(region, pos);
        rec.style      = "waystone";
        rec.facing     = bs::game::opposite(facing);
        rec.origin     = bs::core::Origin::Player;
        rec.ownerUid   = bs::game::Waystones::playerKey(*player);
        rec.ownerName  = player->getRealName();
        rec.visibility = bs::service::get<bs::service::IPermissions>().defaultVisibility();
        auto* r        = bs::game::Waystones::get().create(rec);
        if (!r) return output.error("no world");
        auto uid = r->uid;
        bs::game::Waystones::get().activate(*player, uid);
        bs::log().info("edit: debug waystone {} created at {} {} {}", uid, pos.x, pos.y, pos.z);
        bs::service::get<bs::service::IEditScreen>().open(*player, uid);
        output.success("debug waystone " + uid);
    });
});
