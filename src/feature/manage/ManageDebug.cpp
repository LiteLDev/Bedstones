// Debug subcommands of U11 (op only), used by the e2e scenarios:
//   /waystones debug manage        opens the Manage Waystones screen (command source)
//   /waystones debug manage seed   creates + activates four test waystones in a fixed order
//   /waystones debug manage dump   logs the player's manage order

#include <array>

#include "ll/api/command/CommandHandle.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"

#include "feature/manage/Manage.h"
#include "game/Waystones.h"
#include "mod/Commands.h"
#include "mod/Log.h"

namespace bs::feature::manage {

namespace {

constexpr std::array<char const*, 4> kSeedNames{"Alpha Peak", "Birch Hollow", "Cedar Ford", "Dune Gate"};

core::WarpSource commandSource() {
    core::WarpSource s;
    s.kind = core::WarpSourceKind::Command;
    return s;
}

void seed(Player& player) {
    auto& ws    = game::Waystones::get();
    auto  owner = game::Waystones::playerKey(player);
    auto  feet  = player.getFeetPos();
    std::vector<std::string> uids;
    for (std::size_t i = 0; i < kSeedNames.size(); ++i) {
        auto found = ws.all([&](core::WaystoneRecord const& r) { return r.name == kSeedNames[i] && r.ownerUid == owner; });
        core::WaystoneRecord* r = found.empty() ? nullptr : found.front();
        if (!r) {
            core::WaystoneRecord rec;
            rec.name      = kSeedNames[i];
            rec.style     = "waystone";
            rec.origin    = core::Origin::Player;
            rec.ownerUid  = owner;
            rec.ownerName = player.getRealName();
            // The first one stands within edit range (Edit Waystone button), the rest further away.
            rec.pos = {static_cast<int>(player.getDimensionId()), static_cast<int>(feet.x) + (i == 0 ? 2 : 10 + 3 * static_cast<int>(i)),
                       static_cast<int>(feet.y), static_cast<int>(feet.z)};
            r = ws.create(rec);
        }
        if (!r) return;
        uids.push_back(r->uid);
    }
    auto& data = ws.playerData(player);
    for (auto const& uid : uids) {
        data.activate(uid);
        data.aliases.erase(uid);
        data.groups.erase(uid);
        data.hidden.erase(uid);
        std::erase(data.sortingIndex, uid);
    }
    data.sortingIndex.insert(data.sortingIndex.begin(), uids.begin(), uids.end());
    ws.markPlayerDirty(owner);
    log().info("manage: seeded {} waystones for {}", uids.size(), player.getRealName());
}

void dump(Player& player) {
    std::string order;
    for (auto const& e : entries(player, commandSource())) {
        if (!order.empty()) order += ", ";
        order += e.target.name;
    }
    log().info("manage: order for {}: {}", player.getRealName(), order);
}

} // namespace

} // namespace bs::feature::manage

BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    using namespace bs::feature::manage;
    cmd.overload().text("debug").text("manage").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = bs::commands::playerOf(origin);
        if (!player) return output.error("Only players can open screens.");
        openManage(*player, commandSource());
        output.success("manage screen opened");
    });
    cmd.overload().text("debug").text("manage").text("seed").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = bs::commands::playerOf(origin);
        if (!player || !bs::commands::isAdmin(origin)) return output.error("Only operators can seed test waystones.");
        seed(*player);
        output.success("seeded test waystones");
    });
    cmd.overload().text("debug").text("manage").text("dump").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = bs::commands::playerOf(origin);
        if (!player) return output.error("Only players have a manage order.");
        dump(*player);
        output.success("manage order logged");
    });
});
