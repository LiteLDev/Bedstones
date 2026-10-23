#include "feature/sharestone/Sharestones.h"

#include "mc/world/actor/player/Player.h"

#include "core/Warp.h"
#include "core/Waystone.h"
#include "game/Blocks.h"
#include "game/Waystones.h"
#include "mod/Log.h"
#include "service/Services.h"

namespace bs::feature::sharestone {

namespace {

core::GlobalPos posOf(StoneAt const& s) { return {s.dim, s.x, s.y, s.z}; }

core::WaystoneRecord recordFor(StoneAt const& s) {
    core::WaystoneRecord r;
    r.type       = core::types::sharestone(s.kind.color);
    r.pos        = posOf(s);
    r.style      = core::sharestone::path(s.kind);
    r.facing     = s.facing;
    r.visibility = core::Visibility::Sharestone;
    return r;
}

std::string describe(core::WaystoneRecord const& r) { return r.name.empty() ? r.type : r.name; }

} // namespace

std::string ensureRecord(StoneAt const& s) {
    auto& ws = game::Waystones::get();
    if (!ws.available()) return {};
    auto type = core::types::sharestone(s.kind.color);
    if (auto* r = ws.findAt(posOf(s))) {
        if (r->type == type) return r->uid;
        ws.remove(std::string(r->uid)); // a stale record of another block type at this position
    }
    // A sharestone without a record (set by a command or a structure): Java's block entity
    // initializes one lazily, without an owner.
    auto rec   = recordFor(s);
    rec.origin = core::Origin::Unknown;
    auto* r    = ws.create(std::move(rec));
    if (!r) return {};
    log().info("sharestone record created: {} at {} {} {}", r->type, s.x, s.y, s.z);
    return r->uid;
}

void onPlaced(Player* player, Placer const& placer, StoneAt const& s) {
    auto& ws = game::Waystones::get();
    if (!ws.available()) return;
    if (auto* old = ws.findAt(posOf(s))) ws.remove(std::string(old->uid));
    auto rec      = recordFor(s);
    rec.origin    = core::Origin::Player;
    rec.ownerUid  = placer.uuid;
    rec.ownerName = placer.name;
    auto* r       = ws.create(std::move(rec));
    if (!r) return;
    auto uid = r->uid;
    // Everyone can see sharestones: the record's Created notification is the broadcast.
    log().info("sharestone placed: {} at {} {} {} by {} ({})", r->type, s.x, s.y, s.z, placer.name, uid);
    if (player) service::get<service::IEditScreen>().open(*player, uid);
}

void onUsed(Player& player, StoneAt const& s, bool sneaking) {
    auto uid = ensureRecord(s);
    if (uid.empty()) return;
    auto* r = game::Waystones::get().find(uid);
    if (!r) return;
    if (sneaking) {
        log().info("sharestone edit: {} by {}", describe(*r), player.getRealName());
        service::get<service::IEditScreen>().open(player, uid);
        return;
    }
    core::WarpSource source;
    source.kind    = core::WarpSourceKind::Sharestone;
    source.fromUid = uid;
    source.fromPos = posOf(s);
    log().info("sharestone used: {} by {}", describe(*r), player.getRealName());
    service::get<service::ISelection>().open(player, source);
}

void onRemoved(StoneAt const& s) {
    auto& ws = game::Waystones::get();
    if (!ws.available()) return;
    auto* r = ws.findAt(posOf(s));
    if (!r || r->type != core::types::sharestone(s.kind.color)) return;
    auto uid  = r->uid;
    auto name = describe(*r);
    ws.remove(uid);
    log().info("sharestone removed: {} at {} {} {}", name, s.x, s.y, s.z);
}

} // namespace bs::feature::sharestone
