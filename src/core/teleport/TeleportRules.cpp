#include "core/teleport/TeleportRules.h"

#include <algorithm>
#include <cctype>

namespace bs::core::teleport {

namespace {
bool iequals(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
}
} // namespace

TransportMode transportModeFromString(std::string_view s, TransportMode fallback) {
    if (iequals(s, "ENABLED")) return TransportMode::Enabled;
    if (iequals(s, "SAME_DIMENSION")) return TransportMode::SameDimension;
    if (iequals(s, "DISABLED")) return TransportMode::Disabled;
    return fallback;
}

std::string_view toString(TransportMode m) {
    switch (m) {
    case TransportMode::Enabled:
        return "ENABLED";
    case TransportMode::SameDimension:
        return "SAME_DIMENSION";
    case TransportMode::Disabled:
        return "DISABLED";
    }
    return "DISABLED";
}

bool allowsTransport(TransportMode mode, bool crossDimension) {
    return mode == TransportMode::Enabled || (mode == TransportMode::SameDimension && !crossDimension);
}

std::pair<int, int> offsetOf(int facing) {
    switch (facing & 3) {
    case kSouth:
        return {0, 1};
    case kWest:
        return {-1, 0};
    case kNorth:
        return {0, -1};
    default:
        return {1, 0};
    }
}

float yawOf(int facing) { return static_cast<float>((facing & 3) * 90); }

std::vector<int> landingOrder(int facing) { return {facing & 3, kEast, kWest, kSouth, kNorth}; }

Landing findLanding(int facing, bool onBlock, std::function<bool(int, int)> const& hasSpace) {
    if (!onBlock) {
        for (int dir : landingOrder(facing)) {
            auto [dx, dz] = offsetOf(dir);
            if (!hasSpace || hasSpace(dx, dz)) return {dx, dz, dir};
        }
    }
    return {0, 0, facing & 3};
}

std::optional<LeashError> checkLeashed(TransportMode mode, int leashedCount, bool anyDenied, bool crossDimension) {
    if (leashedCount <= 0) return std::nullopt;
    if (mode == TransportMode::Disabled) return LeashError::Disabled;
    if (anyDenied) return LeashError::DeniedEntity;
    if (crossDimension && mode == TransportMode::SameDimension) return LeashError::DisabledDimension;
    return std::nullopt;
}

std::pair<std::string_view, std::string_view> message(LeashError e) {
    switch (e) {
    case LeashError::Disabled:
        return {"chat.waystones.cannot_transport_leashed", "You cannot take leashed mobs with you"};
    case LeashError::DeniedEntity:
        return {"chat.waystones.cannot_transport_this_leashed", "You cannot take this mob with you"};
    case LeashError::DisabledDimension:
        return {"chat.waystones.cannot_transport_leashed_dimensional", "You cannot take leashed mobs with you between worlds"};
    }
    return {"chat.waystones.cannot_transport_leashed", "You cannot take leashed mobs with you"};
}

bool isDenied(std::vector<std::string> const& denyList, std::string_view entityId) {
    for (auto const& d : denyList) {
        if (iequals(d, entityId)) return true;
        if (d.find(':') == std::string::npos && entityId.starts_with("minecraft:") && iequals(d, entityId.substr(10))) return true;
    }
    return false;
}

bool canInteractWithBlock(double eyeX, double eyeY, double eyeZ, int bx, int by, int bz, double range, double buffer) {
    auto axis = [](double v, int lo) {
        if (v < lo) return lo - v;
        if (v > lo + 1) return v - (lo + 1);
        return 0.0;
    };
    double dx = axis(eyeX, bx), dy = axis(eyeY, by), dz = axis(eyeZ, bz);
    double r  = range + buffer;
    return dx * dx + dy * dy + dz * dz < r * r;
}

bool entityNearBlock(double x, double y, double z, int bx, int by, int bz) {
    double dx = x - (bx + 0.5), dy = y - (by + 0.5), dz = z - (bz + 0.5);
    return dx * dx + dy * dy + dz * dz <= 64.0;
}

std::string expectedBlockPath(std::string_view type, std::string_view style) {
    if (type == "waystone") return style.empty() ? std::string("waystone") : std::string(style);
    return std::string(type);
}

bool isTeleportTargetPath(std::string_view path) {
    if (path.empty()) return false;
    if (path == "warp_plate" || path == "warp_portal" || path == "fleeting_memorial") return true;
    if (path.ends_with("_portstone")) return false;
    return path.ends_with("waystone") || path.ends_with("_sharestone");
}

} // namespace bs::core::teleport
