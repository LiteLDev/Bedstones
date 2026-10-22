#pragma once

// Extra selection-list entries that are not plain waystone records the player activated:
// Twinbound Feather partners, Fleeting Memorials, the return Warp Portal, ...
//
//     BS_REGISTER_TARGET_PROVIDER(bs::service::TargetProvider{
//         .name    = "feather",
//         .list    = [](Player& p, core::WarpSource const& src, std::vector<core::WarpTarget>& out) { ... },
//         .resolve = [](Player& p, std::string const& id) -> std::optional<Destination> { ... },
//         .owns    = [](std::string const& id) { return id.starts_with("player:"); },
//     });
//
// `ITargets::list` calls every provider's `list`; `ITeleport::resolve` asks the provider that
// `owns` an id that is not a waystone record. `onWarped` runs after a successful warp to one of
// the provider's entries (e.g. a memorial destroys itself).

#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core/Warp.h"
#include "mod/Registry.h"
#include "service/Services.h"

class Player;

namespace bs::service {

struct TargetProvider {
    std::string name;
    std::function<void(Player& player, core::WarpSource const& source, std::vector<core::WarpTarget>& out)> list;
    std::function<std::optional<Destination>(Player& player, std::string const& targetId)>                resolve;
    std::function<bool(std::string const& targetId)>                                                      owns;
    std::function<void(Player& player, std::string const& targetId)>                                      onWarped;
};
using TargetProviderRegistry = StaticRegistry<TargetProvider>;

/// The provider owning `targetId`, nullptr if none (then it must be a waystone uid).
[[nodiscard]] TargetProvider const* providerFor(std::string const& targetId);

namespace detail {
struct TargetProviderRegistrar {
    explicit TargetProviderRegistrar(TargetProvider p) { TargetProviderRegistry::add(std::move(p)); }
};
} // namespace detail

} // namespace bs::service

#define BS_REGISTER_TARGET_PROVIDER(...)                                                                               \
    static ::bs::service::detail::TargetProviderRegistrar BS_UNIQUE(bs_target_provider_)(__VA_ARGS__)
