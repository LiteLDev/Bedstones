#pragma once

// Test / debug stand-in for IRequirements with only Waystones' inventory button cooldown
// (`[source_is_inventory_button] add_cooldown(inventory_button, 300)`), so the cooldown path of
// the button can be checked without the requirements unit. Selftests and
// `/waystones inventorybutton debug cooldowns` install it temporarily.

#include <chrono>
#include <cstdint>
#include <memory>

#include "mc/world/actor/player/Player.h"

#include "feature/inventorybutton/InventoryButton.h"
#include "game/Waystones.h"
#include "service/Services.h"

namespace bs::feature::inventorybutton {

struct CooldownRequirements final : service::IRequirements {
    static std::int64_t nowMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch()
        )
            .count();
    }
    core::Requirement resolve(Player& p, core::WarpSource const& source, core::WarpTarget const&) override {
        core::Requirement r;
        if (source.kind != core::WarpSourceKind::InventoryButton) return r;
        r.cooldowns.push_back({kCooldownId, 300, false});
        auto const& cd = game::Waystones::get().playerData(p).cooldowns;
        if (auto it = cd.find(kCooldownId); it != cd.end() && it->second > nowMs()) {
            r.activeCooldown = static_cast<double>(it->second - nowMs()) / 1000.0;
        }
        return r;
    }
    bool canAfford(Player& p, core::Requirement const& r) override {
        return p.isCreative() || (r.refuse.empty() && r.activeCooldown <= 0);
    }
    void consume(Player& p, core::Requirement const& r) override {
        auto& ws = game::Waystones::get();
        for (auto const& c : r.cooldowns)
            ws.playerData(p).cooldowns[c.id] = nowMs() + static_cast<std::int64_t>(c.seconds * 1000);
        ws.markPlayerDirty(game::Waystones::playerKey(p));
    }
};

/// Temporarily replaces a service seam, restoring whatever was installed before (the owning
/// unit's implementation or the default).
template <class I>
struct SeamSwap {
    std::unique_ptr<I> saved;
    explicit SeamSwap(std::unique_ptr<I> impl) {
        saved = std::move(service::detail::slot<I>());
        service::set<I>(std::move(impl));
    }
    ~SeamSwap() { service::detail::slot<I>() = std::move(saved); }
    SeamSwap(SeamSwap const&)            = delete;
    SeamSwap& operator=(SeamSwap const&) = delete;
};

} // namespace bs::feature::inventorybutton
