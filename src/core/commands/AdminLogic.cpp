#include "core/commands/AdminLogic.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace bs::core::commands {

std::string normalizeCooldownId(std::string_view id) {
    std::string out;
    out.reserve(id.size() + 10);
    for (char c : id) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    if (out.empty()) return out;
    if (out.find(':') == std::string::npos) out = "waystones:" + out;
    return out;
}

std::int64_t cooldownAdd(std::int64_t nowMs, std::optional<std::int64_t> current, int seconds) {
    auto base = std::max(nowMs, current.value_or(0));
    return base + static_cast<std::int64_t>(seconds) * 1000;
}

std::int64_t cooldownSet(std::int64_t nowMs, int seconds) { return nowMs + static_cast<std::int64_t>(seconds) * 1000; }

std::string dimensionName(int dim) {
    switch (dim) {
    case 0:
        return "minecraft:overworld";
    case 1:
        return "minecraft:the_nether";
    case 2:
        return "minecraft:the_end";
    default:
        return "minecraft:dimension_" + std::to_string(dim);
    }
}

namespace {
double distanceSq(double x, double y, double z, GlobalPos const& pos) {
    double dx = x - (pos.x + 0.5), dy = y - (pos.y + 0.5), dz = z - (pos.z + 0.5);
    return dx * dx + dy * dy + dz * dz;
}
} // namespace

int blockDistance(double x, double y, double z, GlobalPos const& pos) {
    return static_cast<int>(std::sqrt(distanceSq(x, y, z, pos)));
}

void sortForAdminInspection(std::vector<ListEntry>& entries, std::optional<Viewpoint> const& caller) {
    std::stable_sort(entries.begin(), entries.end(), [&caller](ListEntry const& a, ListEntry const& b) {
        if (a.owned != b.owned) return a.owned;
        if (!caller) return false;
        bool aSame = a.pos.dim == caller->dim, bSame = b.pos.dim == caller->dim;
        if (aSame != bSame) return aSame;
        return distanceSq(caller->x, caller->y, caller->z, a.pos) < distanceSq(caller->x, caller->y, caller->z, b.pos);
    });
}

bool feathersLinked(
    std::optional<std::string> const& firstId,
    std::optional<std::string> const& firstLink,
    std::optional<std::string> const& secondId,
    std::optional<std::string> const& secondLink
) {
    if (!firstId || !secondId || firstId->empty() || secondId->empty()) return false;
    return firstLink == secondId && secondLink == firstId;
}

} // namespace bs::core::commands
