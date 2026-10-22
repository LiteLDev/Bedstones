#include "service/WarpEvents.h"

#include <exception>

#include "mod/Log.h"

namespace bs::service {

std::optional<WarpResult> intercept(Player& player, WarpRequest const& request) {
    for (auto const& fn : WarpInterceptorRegistry::entries()) {
        try {
            if (auto r = fn(player, request)) return r;
        } catch (std::exception const& e) {
            log().error("warp interceptor failed: {}", e.what());
        }
    }
    return std::nullopt;
}

void notifyWarped(Player& player, WarpRequest const& request, Destination const& destination) {
    for (auto const& fn : WarpListenerRegistry::entries()) {
        try {
            fn(player, request, destination);
        } catch (std::exception const& e) {
            log().error("warp listener failed: {}", e.what());
        }
    }
}

} // namespace bs::service
