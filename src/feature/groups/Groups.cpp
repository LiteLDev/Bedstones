// U12 Groups: the IGroups seam implementation and the module that installs it.

#include "feature/groups/Groups.h"

#include <memory>

#include "mc/world/actor/player/Player.h"

#include "game/Waystones.h"
#include "mod/I18n.h"
#include "mod/Module.h"
#include "service/Services.h"

namespace bs::feature::groups {

namespace cg = core::groups;

core::groups::Translator translatorFor(Player const& player) {
    return [&player](std::string const& key, std::string const& fallback) { return trFor(player, key, fallback); };
}

std::vector<core::WaystoneGroup> groupsFor(Player& player) {
    auto const& data = game::Waystones::get().playerData(player);
    auto        list = cg::resolve(data.groupRegistry, translatorFor(player));
    for (auto& g : list) {
        if (g.name.empty()) g.name = trFor(player, "gui.waystones.group_settings.name_placeholder", "Unnamed Group");
    }
    return list;
}

namespace {

class GroupsService final : public service::IGroups {
public:
    std::vector<core::WaystoneGroup> groups(Player& player) override { return groupsFor(player); }

    std::vector<std::string> groupsOf(Player& player, core::WarpTarget const& target) override {
        auto& ws   = game::Waystones::get();
        auto& data = ws.playerData(player);
        bool  team = false;
        if (auto* r = ws.find(target.id)) team = r->visibility == core::Visibility::Team;
        static std::vector<std::string> const kNone;
        auto it       = data.groups.find(target.id);
        auto const& assigned = it != data.groups.end() ? it->second : kNone;
        return cg::groupsOf(data.groupRegistry, assigned, cg::implicitGroups(target.kind, target.global, team, target.pos.dim));
    }
};

class GroupsModule {
public:
    void install() { service::set<service::IGroups>(std::make_unique<GroupsService>()); }
    void uninstall() { service::reset<service::IGroups>(); }
};

} // namespace

} // namespace bs::feature::groups

BS_REGISTER_MODULE(bs::feature::groups::GroupsModule);
