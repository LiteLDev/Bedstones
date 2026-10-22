// Portal Scroll: hold use for 32 ticks (bow animation), then pick a target in the selection menu;
// the Warp Portal itself is opened by the warp interceptor (WarpPortal.cpp).

#include "feature/portal/Portal.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/entity/ActorHurtEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/PlayerItemInUse.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/Level.h"

#include "game/Effects.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "service/Services.h"

namespace bs::feature::portal {

namespace {

/// Waystones' `scrollUseTime` default (ticks of holding use).
constexpr std::uint64_t kChargeTicks = 32;

struct Charge {
    std::uint64_t start = 0;
    bool          over  = false; ///< finished or cancelled: wait for the use to end
};

std::unordered_map<std::string, Charge> gCharges; ///< player uuid -> charge in progress
std::uint64_t                           gTick    = 0;
int                                     gCharged = 0;

bool isScroll(ItemStack const& stack) { return game::idOf(stack) == ids::id(ids::kPortalScroll); }
bool usingItem(Player& player) { return !player.mItemInUse->mItem->isNull(); }
bool usingScroll(Player& player) { return isScroll(player.mItemInUse->mItem); }

// The charge follows the player's item-in-use state (set by the client holding use, the item's
// `minecraft:use_modifiers`), so it works for every way of starting a use.
void tick(Level& level) {
    ++gTick;
    std::unordered_set<std::string> seen;
    level.forEachPlayer([&](Player& player) {
        if (player.isClientSide() || !usingScroll(player)) return true;
        auto key = game::Waystones::playerKey(player);
        seen.insert(key);
        auto it = gCharges.find(key);
        if (it == gCharges.end()) {
            gCharges.emplace(key, Charge{gTick, false});
            game::soundAround(static_cast<int>(player.getDimensionId()), "portal.trigger", player.getPosition(), 0.1f, 2.0f);
            return true;
        }
        auto& c = it->second;
        if (c.over || gTick - c.start < kChargeTicks) return true;
        c.over = true;
        player.stopUsingItem();
        ++gCharged;
        onScrollCharged(player, 0);
        return true;
    });
    // Released early, switched items, died or left.
    std::erase_if(gCharges, [&](auto const& e) { return !seen.contains(e.first); });
}

class PortalScrollModule {
public:
    void install() {
        auto& bus = ll::event::EventBus::getInstance();
        mListeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([](ll::event::ServerLevelTickEvent& ev) {
            tick(ev.level());
        }));
        // Taking damage cancels the charge (docs/SPEC.md §3.13).
        mListeners.push_back(bus.emplaceListener<ll::event::ActorHurtEvent>([](ll::event::ActorHurtEvent& ev) {
            auto& actor = ev.self();
            if (!actor.isPlayer() || gCharges.empty()) return;
            auto& player = static_cast<Player&>(actor);
            auto  it     = gCharges.find(game::Waystones::playerKey(player));
            if (it == gCharges.end() || it->second.over) return;
            it->second.over = true;
            if (usingItem(player)) player.stopUsingItem();
        }));
    }
    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
        gCharges.clear();
    }

private:
    std::vector<ll::event::ListenerPtr> mListeners;
};

} // namespace

int chargeCount() { return gCharged; }

bool onScrollCharged(Player& player, int hand) {
    if (!portalSpotFor(player)) {
        showNoSpace(player);
        log().info("portal scroll: no space for {}", player.getRealName());
        return false;
    }
    core::WarpSource source;
    source.kind   = core::WarpSourceKind::PortalScroll;
    source.itemId = ids::id(ids::kPortalScroll);
    source.hand   = hand;
    log().info("portal scroll charged by {}", player.getRealName());
    service::get<service::ISelection>().open(player, source);
    return true;
}

} // namespace bs::feature::portal

BS_REGISTER_MODULE(bs::feature::portal::PortalScrollModule);
