#include "feature/portstone/Portstones.h"

#include "mc/world/actor/player/Player.h"

#include "core/Warp.h"
#include "mod/Log.h"
#include "service/Services.h"

namespace bs::feature::portstone {

void onPlaced(Placer const& placer, StoneAt const& s) {
    // Portstones have no record: they can not be teleported to.
    log().info("portstone placed: {} at {} {} {} by {}", s.kind.color, s.x, s.y, s.z, placer.name);
}

void onUsed(Player& player, StoneAt const& s) {
    core::WarpSource source;
    source.kind           = core::WarpSourceKind::Portstone;
    source.portstoneColor = s.kind.color;
    source.fromPos        = {s.dim, s.x, s.y, s.z};
    log().info("portstone used: {} by {}", s.kind.color, player.getRealName());
    service::get<service::ISelection>().open(player, source);
}

} // namespace bs::feature::portstone
