#include "service/ScreenOpeners.h"

#include <exception>

#include "mod/Log.h"

namespace bs::service {

bool openScreen(Player& player, std::string const& name, nlohmann::json const& args) {
    for (auto const& o : ScreenOpenerRegistry::entries()) {
        if (o.name != name || !o.open) continue;
        try {
            o.open(player, args);
        } catch (std::exception const& e) {
            log().error("opening screen {} failed: {}", name, e.what());
        }
        return true;
    }
    log().debug("screen {} is not available (its unit is not installed)", name);
    return false;
}

bool hasScreen(std::string const& name) {
    for (auto const& o : ScreenOpenerRegistry::entries()) {
        if (o.name == name && o.open) return true;
    }
    return false;
}

} // namespace bs::service
