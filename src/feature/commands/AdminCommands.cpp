// Waystones' admin commands (docs/SPEC.md §6). See AdminCommands.h.

#include "feature/commands/AdminCommands.h"

#include <chrono>

#include "ll/api/command/CommandHandle.h"
#include "ll/api/command/SoftEnum.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/server/commands/CommandPosition.h"
#include "mc/server/commands/CommandRawText.h"
#include "mc/server/commands/CommandSelector.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"

#include "game/Blocks.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/Commands.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "service/ScreenOpeners.h"
#include "service/Services.h"

namespace bs::feature::commands {

using core::GlobalPos;
using core::WaystoneRecord;

// ---- command parameter types (external linkage: command reflection) --------------------------------

/// Suggestions of `place <pos> <style>` (the 10 waystone styles); any word is accepted so unknown
/// styles reach the "Unknown waystone style" message.
enum class WaystoneStyle {
    waystone,
    mossy_waystone,
    sandy_waystone,
    deepslate_waystone,
    blackstone_waystone,
    end_stone_waystone,
    red_nether_bricks_waystone,
    purpur_waystone,
    prismarine_waystone,
    mud_bricks_waystone,
};
/// Suggestions of the cooldown id argument.
enum class WaystoneCooldownId { inventory_button };

struct TargetsPosParams {
    CommandSelector<Player> targets;
    CommandPosition         pos;
};
struct TargetsParams {
    CommandSelector<Player> targets;
};
struct PlaceParams {
    CommandPosition                          pos;
    ll::command::SoftEnum<WaystoneStyle>     style;
    CommandRawText                           name;
};
struct PlayerParams {
    CommandSelector<Player> player;
};
struct TwinboundParams {
    CommandSelector<Player> first;
    CommandSelector<Player> second;
};
struct CooldownIdParams {
    CommandSelector<Player>                   targets;
    ll::command::SoftEnum<WaystoneCooldownId> id;
};
struct CooldownSecondsParams {
    CommandSelector<Player>                   targets;
    ll::command::SoftEnum<WaystoneCooldownId> id;
    int                                       seconds = 0;
};

namespace {

std::string gFeatherItem; // "" = bedstones:twinbound_feather

std::string featherItem() { return gFeatherItem.empty() ? ids::id(ids::kTwinboundFeather) : gFeatherItem; }

template <class... Args>
std::string say(Player* viewer, std::string_view key, std::string_view fallback, Args const&... args) {
    return viewer ? trFor(*viewer, key, fallback, args...) : tr(key, fallback, args...);
}

Reply fail(std::string line) { return Reply{false, {std::move(line)}}; }
Reply ok(std::string line) { return Reply{true, {std::move(line)}}; }

Reply notFound(Player* viewer) {
    return fail(say(viewer, "commands.waystones.waystone_not_found", "Waystone not found at this position"));
}

BlockSource* regionOf(int dim) {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    auto d = level->getDimension(dim).lock();
    return d ? &d->getBlockSourceFromMainChunkSource() : nullptr;
}

/// "Activated waystone %s for %s" / "... for %d players".
Reply perTargets(Player* viewer, std::vector<Player*> const& targets, std::string const& base, std::string_view singleFallback, std::string_view multipleFallback, auto const&... args) {
    if (targets.size() == 1) {
        return ok(say(viewer, base + ".success.single", singleFallback, args..., targets.front()->getRealName()));
    }
    return ok(say(viewer, base + ".success.multiple", multipleFallback, args..., targets.size()));
}

std::string waystoneName(WaystoneRecord const& r) { return r.name; }

} // namespace

std::string Reply::text() const {
    std::string out;
    for (auto const& l : lines) {
        if (!out.empty()) out += '\n';
        out += l;
    }
    return out;
}

std::int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

WaystoneRecord* waystoneAt(GlobalPos const& pos) {
    auto& ws = game::Waystones::get();
    if (!ws.available()) return nullptr;
    if (auto* r = ws.findAt(pos)) return r;
    if (auto* region = regionOf(pos.dim)) {
        auto lower = game::lowerHalf(*region, game::blockPos(pos));
        if (lower.y != pos.y) return ws.findAt(game::globalPos(pos.dim, lower));
    }
    return nullptr;
}

// ---- activate / forget ---------------------------------------------------------------------------------

Reply activate(Player* viewer, std::vector<Player*> const& targets, GlobalPos const& pos) {
    auto* r = waystoneAt(pos);
    if (!r) return notFound(viewer);
    auto& ws  = game::Waystones::get();
    auto  uid = r->uid;
    for (auto* p : targets) ws.activate(*p, uid);
    log().info("commands: activated waystone {} for {} players", r->name, targets.size());
    return perTargets(viewer, targets, "commands.waystones.activate", "Activated waystone {} for {}", "Activated waystone {} for {} players", waystoneName(*r));
}

Reply forget(Player* viewer, std::vector<Player*> const& targets, GlobalPos const& pos) {
    auto* r = waystoneAt(pos);
    if (!r) return notFound(viewer);
    auto& ws  = game::Waystones::get();
    auto  uid = r->uid;
    for (auto* p : targets) ws.forget(*p, uid);
    log().info("commands: deactivated waystone {} for {} players", r->name, targets.size());
    return perTargets(viewer, targets, "commands.waystones.forget", "Deactivated waystone {} for {}", "Deactivated waystone {} for {} players", waystoneName(*r));
}

Reply forgetAll(Player* viewer, std::vector<Player*> const& targets) {
    auto& ws = game::Waystones::get();
    for (auto* p : targets) {
        auto uids = ws.playerData(*p).activated; // copy: forget() edits the list
        for (auto const& uid : uids) ws.forget(*p, uid);
    }
    log().info("commands: deactivated all waystones for {} players", targets.size());
    return perTargets(viewer, targets, "commands.waystones.forget.all", "Deactivated all waystones for {}", "Deactivated all waystones for {} players");
}

// ---- place ------------------------------------------------------------------------------------------------

Reply place(Player* viewer, GlobalPos const& pos, std::string const& style, std::string const& name) {
    std::string path = style;
    if (auto p = ids::pathOf(style); !p.empty()) path = std::string(p);
    else if (style.starts_with("waystones:")) path = style.substr(10);
    if (!ids::isWaystoneStyle(path)) {
        return fail(say(viewer, "bs.commands.unknown_style", "Unknown waystone style: {}", style));
    }
    auto& ws = game::Waystones::get();
    if (!ws.available()) return fail(say(viewer, "bs.commands.no_world", "No world is loaded"));
    auto* region = regionOf(pos.dim);
    if (!region) return fail(say(viewer, "bs.commands.no_world", "No world is loaded"));

    auto blockId = ids::id(path);
    int  facing  = 2; // Java's default state faces north
    bool hasBlock = game::blockWith(blockId) != nullptr;
    bool placed   = false;
    if (hasBlock) {
        auto bp = game::blockPos(pos);
        // The command replaces whatever is there, like Java's setBlock.
        game::clearBlock(*region, bp);
        game::clearBlock(*region, BlockPos{bp.x, bp.y + 1, bp.z});
        placed = game::placeTwoTall(*region, bp, blockId, facing);
    }
    if (hasBlock && !placed) return fail(say(viewer, "bs.commands.place.failed", "Could not place {} here", blockId));

    // A block placed over an old record replaces it.
    if (auto* old = ws.findAt(pos)) ws.remove(std::string(old->uid));
    WaystoneRecord rec;
    rec.type       = std::string(core::types::kWaystone);
    rec.name       = name;
    rec.pos        = pos;
    rec.style      = path;
    rec.facing     = facing;
    rec.origin     = core::Origin::Player;
    rec.visibility = service::get<service::IPermissions>().defaultVisibility();
    auto* created  = ws.create(std::move(rec));
    if (!created) return fail(say(viewer, "bs.commands.no_world", "No world is loaded"));
    log().info(
        "commands: placed waystone {} ({}) at {} {} {} dim {}{}",
        name,
        path,
        pos.x,
        pos.y,
        pos.z,
        pos.dim,
        placed ? "" : " (record only, block missing)"
    );
    if (!placed) {
        return Reply{true, {say(viewer, "bs.commands.place.missing_block", "Block {} is not registered: created the waystone record only", blockId)}};
    }
    return Reply{true, {}}; // Waystones prints nothing on success
}

// ---- count / list -----------------------------------------------------------------------------------------

namespace {
struct Owned {
    std::vector<WaystoneRecord*> activated;
    std::size_t                  owned = 0;
};
Owned activatedOf(Player& target) {
    auto&  ws  = game::Waystones::get();
    auto   key = game::Waystones::playerKey(target);
    Owned  out;
    for (auto const& uid : ws.playerData(target).activated) {
        auto* r = ws.find(uid);
        if (!r) continue;
        out.activated.push_back(r);
        if (r->ownerUid == key) ++out.owned;
    }
    return out;
}
} // namespace

Reply count(Player* viewer, Player& target) {
    auto data = activatedOf(target);
    log().info("commands: count {} -> {} activated, {} owned", target.getRealName(), data.activated.size(), data.owned);
    return ok(say(
        viewer,
        "commands.waystones.count",
        "{} has {} waystones activated and owns {} of those",
        target.getRealName(),
        data.activated.size(),
        data.owned
    ));
}

Reply list(Player* viewer, std::optional<core::commands::Viewpoint> const& from, Player& target, bool all) {
    auto data = activatedOf(target);
    auto key  = game::Waystones::playerKey(target);
    auto& pd  = game::Waystones::get().playerData(target);
    std::vector<core::commands::ListEntry> entries;
    for (auto* r : data.activated) {
        bool owned = r->ownerUid == key;
        if (!all && !owned) continue;
        entries.push_back({r->uid, pd.effectiveName(r->uid, r->name), r->pos, owned});
    }
    core::commands::sortForAdminInspection(entries, from);

    Reply reply;
    auto  name = target.getRealName();
    reply.lines.push_back(
        all ? say(viewer, "commands.waystones.list.all.header", "Waystones (x, y, z) by {}:", name)
            : say(viewer, "commands.waystones.list.header", "Waystones (x, y, z) by {} (owned only):", name)
    );
    for (auto const& e : entries) {
        std::string location;
        if (!from || from->dim != e.pos.dim) {
            location = say(viewer, "commands.waystones.list.in_dimension", "in {}", core::commands::dimensionName(e.pos.dim));
        } else {
            location = say(viewer, "commands.waystones.list.at_distance", "at {} blocks away", core::commands::blockDistance(from->x, from->y, from->z, e.pos));
        }
        // Java: coordinates yellow, name green (both click-to-suggest a teleport; Bedrock chat
        // has no click events).
        auto coords = "§e" + say(viewer, "commands.waystones.list.coordinates", "{}, {}, {}", e.pos.x, e.pos.y, e.pos.z) + "§r";
        auto label  = "§a" + e.name + "§r";
        reply.lines.push_back(
            e.owned ? say(viewer, "commands.waystones.list.entry.owned", "-     owned {} ({}): {}", location, coords, label)
                    : say(viewer, "commands.waystones.list.entry.activated", "- activated {} ({}): {}", location, coords, label)
        );
    }
    reply.lines.push_back(
        all ? say(viewer, "commands.waystones.list.all.footer", "{} waystones total, {} owned", entries.size(), data.owned)
            : say(viewer, "commands.waystones.list.footer", "{} waystones owned", entries.size())
    );
    log().info("commands: list {}{} -> {} entries", name, all ? " all" : "", entries.size());
    return reply;
}

// ---- gui ----------------------------------------------------------------------------------------------------

Reply gui(Player& viewer, Player& target) {
    auto owner = game::Waystones::playerKey(target);
    if (service::hasScreen("admin_selection")) {
        service::openScreen(viewer, "admin_selection", {{"owner", owner}});
    } else {
        service::get<service::ISelection>().openAdmin(viewer, owner);
    }
    log().info("commands: gui {} opened for {}", target.getRealName(), viewer.getRealName());
    return Reply{true, {}};
}

// ---- twinbound ----------------------------------------------------------------------------------------------

namespace {
int freeSlot(Player& p) {
    int size = p.getInventory().getContainerSize();
    for (int i = 0; i < size; ++i) {
        if (game::itemAt(p, {game::ItemLocation::Kind::Inventory, i}).isNull()) return i;
    }
    return -1;
}
std::optional<game::ItemLocation> carriedFeather(Player& p) {
    auto id = featherItem();
    return game::findItem(p, [&id](ItemStack const& s) { return game::idOf(s) == id; });
}
} // namespace

Reply twinbound(Player* viewer, Player& first, Player& second) {
    if (&first == &second || game::Waystones::playerKey(first) == game::Waystones::playerKey(second)) {
        return fail(say(viewer, "commands.waystones.twinbound.same_player", "Choose two different players"));
    }
    auto firstLoc  = carriedFeather(first);
    auto secondLoc = carriedFeather(second);
    for (auto [p, loc] : {std::pair{&first, &firstLoc}, std::pair{&second, &secondLoc}}) {
        if (!*loc && freeSlot(*p) < 0) {
            return fail(say(viewer, "commands.waystones.twinbound.no_space", "{} needs a free inventory slot for a Twinbound Feather", p->getRealName()));
        }
    }
    if ((!firstLoc || !secondLoc) && game::makeItem(featherItem()).isNull()) {
        return fail(say(viewer, "bs.commands.twinbound.missing_item", "Item {} is not registered", featherItem()));
    }
    // Give the missing feathers.
    for (auto [p, loc] : {std::pair{&first, &firstLoc}, std::pair{&second, &secondLoc}}) {
        if (*loc) continue;
        int slot = freeSlot(*p);
        game::setItemAt(*p, {game::ItemLocation::Kind::Inventory, slot}, game::makeItem(featherItem()));
        *loc = game::ItemLocation{game::ItemLocation::Kind::Inventory, slot};
    }
    ItemStack a = game::itemAt(first, *firstLoc);
    ItemStack b = game::itemAt(second, *secondLoc);
    auto aId = game::userString(a, ids::kNbtFeather), bId = game::userString(b, ids::kNbtFeather);
    if (core::commands::feathersLinked(aId, game::userString(a, ids::kNbtFeatherLink), bId, game::userString(b, ids::kNbtFeatherLink))) {
        log().info("commands: twinbound {} and {} already linked", first.getRealName(), second.getRealName());
        return fail(say(viewer, "commands.waystones.twinbound.already_linked", "{} and {} already have linked Twinbound Feathers", first.getRealName(), second.getRealName()));
    }
    if (!aId) aId = core::randomUuid();
    if (!bId) bId = core::randomUuid();
    game::setUserString(a, ids::kNbtFeather, *aId);
    game::setUserString(a, ids::kNbtFeatherLink, *bId);
    game::setUserString(b, ids::kNbtFeather, *bId);
    game::setUserString(b, ids::kNbtFeatherLink, *aId);
    game::setItemAt(first, *firstLoc, a);
    game::setItemAt(second, *secondLoc, b);
    log().info("commands: twinbound linked {} and {}", first.getRealName(), second.getRealName());
    return ok(say(viewer, "commands.waystones.twinbound.success", "Linked Twinbound Feathers for {} and {}", first.getRealName(), second.getRealName()));
}

// ---- cooldown -----------------------------------------------------------------------------------------------

namespace {
template <class Fn>
void editCooldowns(std::vector<Player*> const& targets, Fn&& fn) {
    auto& ws = game::Waystones::get();
    for (auto* p : targets) {
        fn(ws.playerData(*p).cooldowns);
        ws.markPlayerDirty(game::Waystones::playerKey(*p));
    }
}
} // namespace

Reply cooldownResetAll(Player* viewer, std::vector<Player*> const& targets) {
    editCooldowns(targets, [](auto& c) { c.clear(); });
    log().info("commands: cooldown reset all for {} players", targets.size());
    return perTargets(viewer, targets, "commands.waystones.cooldown.reset.all", "Reset all cooldowns for {}", "Reset all cooldowns for {} players");
}

Reply cooldownReset(Player* viewer, std::vector<Player*> const& targets, std::string const& rawId) {
    auto id = core::commands::normalizeCooldownId(rawId);
    editCooldowns(targets, [&id](auto& c) { c.erase(id); });
    log().info("commands: cooldown reset {} for {} players", id, targets.size());
    return perTargets(viewer, targets, "commands.waystones.cooldown.reset", "Reset cooldown {} for {}", "Reset cooldown {} for {} players", id);
}

Reply cooldownAdd(Player* viewer, std::vector<Player*> const& targets, std::string const& rawId, int seconds) {
    auto id  = core::commands::normalizeCooldownId(rawId);
    auto now = nowMs();
    editCooldowns(targets, [&](auto& c) {
        auto it = c.find(id);
        c[id]   = core::commands::cooldownAdd(now, it != c.end() ? std::optional{it->second} : std::nullopt, seconds);
    });
    log().info("commands: cooldown add {} {}s for {} players", id, seconds, targets.size());
    return perTargets(viewer, targets, "commands.waystones.cooldown.add", "Added {} seconds to cooldown {} for {}", "Added {} seconds to cooldown {} for {} players", seconds, id);
}

Reply cooldownSet(Player* viewer, std::vector<Player*> const& targets, std::string const& rawId, int seconds) {
    if (seconds < 0) return fail(say(viewer, "bs.commands.cooldown.negative", "Integer must not be less than 0, found {}", seconds));
    auto id  = core::commands::normalizeCooldownId(rawId);
    auto now = nowMs();
    editCooldowns(targets, [&](auto& c) { c[id] = core::commands::cooldownSet(now, seconds); });
    log().info("commands: cooldown set {} {}s for {} players", id, seconds, targets.size());
    return perTargets(viewer, targets, "commands.waystones.cooldown.set", "Set cooldown {} to {} seconds for {}", "Set cooldown {} to {} seconds for {} players", id, seconds);
}

namespace detail {
void setFeatherItemForTest(std::string id) { gFeatherItem = std::move(id); }
} // namespace detail

// ---- command registration -----------------------------------------------------------------------------------

namespace {

void print(CommandOutput& out, Reply const& r) {
    if (r.ok) {
        if (r.lines.empty()) ++out.mSuccessCount; // succeeded silently
        for (auto const& l : r.lines) out.success(l);
    } else {
        for (auto const& l : r.lines) out.error(l);
    }
}

/// Permission level 2 (Waystones: `waystones:command.waystones.<sub>`, level 2).
bool gate(CommandOrigin const& origin, CommandOutput& out) {
    if (bs::commands::isAdmin(origin)) return true;
    Player* p = bs::commands::playerOf(origin);
    out.error(say(p, "bs.commands.no_permission", "You do not have permission to use this command"));
    return false;
}

std::vector<Player*> resolve(CommandSelector<Player> const& sel, CommandOrigin const& origin) {
    std::vector<Player*> out;
    for (auto* p : sel.results(origin)) {
        if (p) out.push_back(p);
    }
    return out;
}

/// Players for a multi-target argument; prints "No player was found" when empty.
std::optional<std::vector<Player*>> targetsOf(CommandSelector<Player> const& sel, CommandOrigin const& origin, CommandOutput& out) {
    auto v = resolve(sel, origin);
    if (v.empty()) {
        out.error(say(bs::commands::playerOf(origin), "bs.commands.no_player", "No player was found"));
        return std::nullopt;
    }
    return v;
}

/// Exactly one player (Java's EntityArgument.player()).
Player* singleOf(CommandSelector<Player> const& sel, CommandOrigin const& origin, CommandOutput& out) {
    auto  v      = resolve(sel, origin);
    auto* viewer = bs::commands::playerOf(origin);
    if (v.empty()) {
        out.error(say(viewer, "bs.commands.no_player", "No player was found"));
        return nullptr;
    }
    if (v.size() > 1) {
        out.error(say(viewer, "bs.commands.too_many_players", "Only one player is allowed, but the provided selector allows more than one"));
        return nullptr;
    }
    return v.front();
}

GlobalPos posOf(CommandPosition const& pos, CommandOrigin const& origin, ::Command const& cmd) {
    auto bp  = pos.getBlockPos(cmd.mVersion, origin, Vec3{0, 0, 0});
    int  dim = 0;
    if (auto* d = origin.getDimension()) dim = static_cast<int>(d->getDimensionId());
    return game::globalPos(dim, bp);
}

std::optional<core::commands::Viewpoint> viewpointOf(CommandOrigin const& origin) {
    auto* p = bs::commands::playerOf(origin);
    if (!p) return std::nullopt;
    auto feet = p->getFeetPos();
    return core::commands::Viewpoint{static_cast<int>(p->getDimensionId()), feet.x, feet.y, feet.z};
}

void registerOverloads(ll::command::CommandHandle& cmd) {
    using bs::commands::playerOf;

    cmd.overload<TargetsPosParams>().text("activate").required("targets").required("pos").execute(
        [](CommandOrigin const& o, CommandOutput& out, TargetsPosParams const& a, ::Command const& c) {
            if (!gate(o, out)) return;
            if (auto t = targetsOf(a.targets, o, out)) print(out, activate(playerOf(o), *t, posOf(a.pos, o, c)));
        }
    );
    cmd.overload<TargetsParams>().text("forget").required("targets").text("all").execute(
        [](CommandOrigin const& o, CommandOutput& out, TargetsParams const& a) {
            if (!gate(o, out)) return;
            if (auto t = targetsOf(a.targets, o, out)) print(out, forgetAll(playerOf(o), *t));
        }
    );
    cmd.overload<TargetsPosParams>().text("forget").required("targets").required("pos").execute(
        [](CommandOrigin const& o, CommandOutput& out, TargetsPosParams const& a, ::Command const& c) {
            if (!gate(o, out)) return;
            if (auto t = targetsOf(a.targets, o, out)) print(out, forget(playerOf(o), *t, posOf(a.pos, o, c)));
        }
    );
    cmd.overload<PlaceParams>().text("place").required("pos").required("style").required("name").execute(
        [](CommandOrigin const& o, CommandOutput& out, PlaceParams const& a, ::Command const& c) {
            if (!gate(o, out)) return;
            print(out, place(playerOf(o), posOf(a.pos, o, c), std::string(a.style), a.name.mText));
        }
    );
    cmd.overload<PlayerParams>().text("count").required("player").execute(
        [](CommandOrigin const& o, CommandOutput& out, PlayerParams const& a) {
            if (!gate(o, out)) return;
            if (auto* p = singleOf(a.player, o, out)) print(out, count(playerOf(o), *p));
        }
    );
    cmd.overload<PlayerParams>().text("list").required("player").execute(
        [](CommandOrigin const& o, CommandOutput& out, PlayerParams const& a) {
            if (!gate(o, out)) return;
            if (auto* p = singleOf(a.player, o, out)) print(out, list(playerOf(o), viewpointOf(o), *p, false));
        }
    );
    cmd.overload<PlayerParams>().text("list").required("player").text("all").execute(
        [](CommandOrigin const& o, CommandOutput& out, PlayerParams const& a) {
            if (!gate(o, out)) return;
            if (auto* p = singleOf(a.player, o, out)) print(out, list(playerOf(o), viewpointOf(o), *p, true));
        }
    );
    cmd.overload<PlayerParams>().text("gui").required("player").execute(
        [](CommandOrigin const& o, CommandOutput& out, PlayerParams const& a) {
            if (!gate(o, out)) return;
            auto* viewer = playerOf(o);
            if (!viewer) return out.error(tr("bs.commands.player_required", "A player is required to run this command here"));
            if (auto* p = singleOf(a.player, o, out)) print(out, gui(*viewer, *p));
        }
    );
    cmd.overload<TwinboundParams>().text("twinbound").required("first").required("second").execute(
        [](CommandOrigin const& o, CommandOutput& out, TwinboundParams const& a) {
            if (!gate(o, out)) return;
            auto* first = singleOf(a.first, o, out);
            if (!first) return;
            auto* second = singleOf(a.second, o, out);
            if (!second) return;
            print(out, twinbound(playerOf(o), *first, *second));
        }
    );
    cmd.overload<TargetsParams>().text("cooldown").required("targets").text("reset").text("all").execute(
        [](CommandOrigin const& o, CommandOutput& out, TargetsParams const& a) {
            if (!gate(o, out)) return;
            if (auto t = targetsOf(a.targets, o, out)) print(out, cooldownResetAll(playerOf(o), *t));
        }
    );
    cmd.overload<CooldownIdParams>().text("cooldown").required("targets").text("reset").required("id").execute(
        [](CommandOrigin const& o, CommandOutput& out, CooldownIdParams const& a) {
            if (!gate(o, out)) return;
            auto t = targetsOf(a.targets, o, out);
            if (!t) return;
            std::string id(a.id);
            print(out, id == "all" ? cooldownResetAll(playerOf(o), *t) : cooldownReset(playerOf(o), *t, id));
        }
    );
    cmd.overload<CooldownSecondsParams>().text("cooldown").required("targets").text("add").required("id").required("seconds").execute(
        [](CommandOrigin const& o, CommandOutput& out, CooldownSecondsParams const& a) {
            if (!gate(o, out)) return;
            if (auto t = targetsOf(a.targets, o, out)) print(out, cooldownAdd(playerOf(o), *t, std::string(a.id), a.seconds));
        }
    );
    cmd.overload<CooldownSecondsParams>().text("cooldown").required("targets").text("set").required("id").required("seconds").execute(
        [](CommandOrigin const& o, CommandOutput& out, CooldownSecondsParams const& a) {
            if (!gate(o, out)) return;
            if (auto t = targetsOf(a.targets, o, out)) print(out, cooldownSet(playerOf(o), *t, std::string(a.id), a.seconds));
        }
    );
}

} // namespace

} // namespace bs::feature::commands

BS_REGISTER_SUBCOMMAND(bs::feature::commands::registerOverloads);
