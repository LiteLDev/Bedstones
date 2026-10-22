#pragma once

// Hooks around a warp, honoured by every ITeleport implementation (the default and U7's):
//
// 1. Interceptors run first (after validation, before paying requirements). One may take the warp
//    over and return a result, e.g. the Portal Scroll spawns a Warp Portal to the chosen target
//    instead of teleporting the player:
//        BS_REGISTER_WARP_INTERCEPTOR([](Player& p, WarpRequest const& r) -> std::optional<WarpResult> {
//            if (r.source.kind != core::WarpSourceKind::PortalScroll) return std::nullopt;
//            ...; return WarpResult::success();
//        });
// 2. Listeners run after a successful teleport, e.g. consuming the scroll / damaging the warp
//    stone that started it (source.itemId / source.hand), or achievements-like bookkeeping:
//        BS_REGISTER_WARP_LISTENER([](Player& p, WarpRequest const& r, Destination const& d) { ... });
//
// An ITeleport implementation calls `intercept()` and `notifyWarped()`; nobody else does.

#include <functional>
#include <optional>

#include "mod/Registry.h"
#include "service/Services.h"

class Player;

namespace bs::service {

using WarpInterceptor = std::function<std::optional<WarpResult>(Player& player, WarpRequest const& request)>;
using WarpListener    = std::function<void(Player& player, WarpRequest const& request, Destination const& destination)>;

using WarpInterceptorRegistry = StaticRegistry<WarpInterceptor>;
using WarpListenerRegistry    = StaticRegistry<WarpListener>;

/// The first interceptor's result, nullopt when none takes the warp over.
[[nodiscard]] std::optional<WarpResult> intercept(Player& player, WarpRequest const& request);
/// Runs every listener (after a successful teleport).
void notifyWarped(Player& player, WarpRequest const& request, Destination const& destination);

namespace detail {
struct WarpInterceptorRegistrar {
    explicit WarpInterceptorRegistrar(WarpInterceptor fn) { WarpInterceptorRegistry::add(std::move(fn)); }
};
struct WarpListenerRegistrar {
    explicit WarpListenerRegistrar(WarpListener fn) { WarpListenerRegistry::add(std::move(fn)); }
};
} // namespace detail

} // namespace bs::service

#define BS_REGISTER_WARP_INTERCEPTOR(...)                                                                              \
    static ::bs::service::detail::WarpInterceptorRegistrar BS_UNIQUE(bs_warp_interceptor_)(__VA_ARGS__)
#define BS_REGISTER_WARP_LISTENER(...)                                                                                 \
    static ::bs::service::detail::WarpListenerRegistrar BS_UNIQUE(bs_warp_listener_)(__VA_ARGS__)
