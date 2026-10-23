#include "core/inventorybutton/InventoryButton.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>

namespace bs::core::inventorybutton {

namespace {
bool iequals(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}
} // namespace

std::string_view trim(std::string_view config) {
    while (!config.empty() && std::isspace(static_cast<unsigned char>(config.front()))) config.remove_prefix(1);
    while (!config.empty() && std::isspace(static_cast<unsigned char>(config.back()))) config.remove_suffix(1);
    return config;
}

Mode parseMode(std::string_view config) {
    // A stray space must not turn NEAREST into a waystone name.
    config = trim(config);
    if (config.empty() || iequals(config, "NONE")) return Mode::None;
    if (iequals(config, "NEAREST")) return Mode::Nearest;
    if (iequals(config, "ANY")) return Mode::Any;
    return Mode::Named;
}

std::string_view toString(Mode m) {
    switch (m) {
    case Mode::Nearest:
        return "nearest";
    case Mode::Any:
        return "any";
    case Mode::Named:
        return "named";
    case Mode::None:
        break;
    }
    return "none";
}

Mode modeFromString(std::string_view s) {
    if (s == "nearest") return Mode::Nearest;
    if (s == "any") return Mode::Any;
    if (s == "named") return Mode::Named;
    return Mode::None;
}

std::optional<std::size_t> nearest(std::vector<Candidate> const& candidates, int dim, double x, double y, double z) {
    std::optional<std::size_t> best;
    double                     bestDist = std::numeric_limits<double>::infinity();
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        auto const& p = candidates[i].pos;
        if (p.dim != dim) continue;
        double dx = p.x + 0.5 - x, dy = p.y + 0.5 - y, dz = p.z + 0.5 - z;
        double d = dx * dx + dy * dy + dz * dz;
        if (d < bestDist) {
            bestDist = d;
            best     = i;
        }
    }
    return best;
}

std::optional<std::size_t> byName(std::vector<Candidate> const& candidates, std::string_view name) {
    if (name.empty()) return std::nullopt;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        if (candidates[i].name == name) return i;
    }
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        if (!candidates[i].alias.empty() && candidates[i].alias == name) return i;
    }
    return std::nullopt;
}

int cooldownSeconds(double activeCooldown) {
    if (!(activeCooldown > 0)) return 0;
    return static_cast<int>(std::ceil(activeCooldown - 1e-9));
}

nlohmann::json ButtonState::toJson() const {
    return {
        {"mode",        std::string(toString(mode))},
        {"hasTarget",   hasTarget                  },
        {"targetId",    targetId                   },
        {"targetName",  targetName                 },
        {"canAfford",   canAfford                  },
        {"cooldown",    cooldownSeconds            },
        {"requirement", requirement.toJson()       },
        {"x",           x                          },
        {"y",           y                          },
        {"creativeX",   creativeX                  },
        {"creativeY",   creativeY                  },
    };
}

ButtonState ButtonState::fromJson(nlohmann::json const& j) {
    ButtonState s;
    if (!j.is_object()) return s;
    s.mode            = modeFromString(j.value("mode", std::string{}));
    s.hasTarget       = j.value("hasTarget", false);
    s.targetId        = j.value("targetId", std::string{});
    s.targetName      = j.value("targetName", std::string{});
    s.canAfford       = j.value("canAfford", true);
    s.cooldownSeconds = j.value("cooldown", 0);
    if (auto it = j.find("requirement"); it != j.end()) s.requirement = Requirement::fromJson(*it);
    s.x         = j.value("x", 58);
    s.y         = j.value("y", 60);
    s.creativeX = j.value("creativeX", 88);
    s.creativeY = j.value("creativeY", 33);
    return s;
}

} // namespace bs::core::inventorybutton
