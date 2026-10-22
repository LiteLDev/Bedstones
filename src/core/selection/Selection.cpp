#include "core/selection/Selection.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace bs::core::selection {

std::string_view toString(SortMode m) {
    switch (m) {
    case SortMode::Name:
        return "name";
    case SortMode::Distance:
        return "distance";
    case SortMode::Manual:
    default:
        return "manual";
    }
}

std::optional<SortMode> sortModeFromString(std::string_view s) {
    if (s == "manual") return SortMode::Manual;
    if (s == "name") return SortMode::Name;
    if (s == "distance") return SortMode::Distance;
    return std::nullopt;
}

SortMode nextSortMode(SortMode m) {
    switch (m) {
    case SortMode::Manual:
        return SortMode::Name;
    case SortMode::Name:
        return SortMode::Distance;
    case SortMode::Distance:
    default:
        return SortMode::Manual;
    }
}

double distanceTo(Viewpoint const& from, GlobalPos const& pos) {
    double dx = pos.x + 0.5 - from.x;
    double dy = pos.y + 0.5 - from.y;
    double dz = pos.z + 0.5 - from.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool isPinned(WarpTarget const& t) {
    return t.pinned || t.kind == types::kWarpPortal || t.kind == types::kFleetingMemorial;
}

std::string toLower(std::string_view s) {
    std::string out(s);
    for (auto& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

void sortTargets(std::vector<WarpTarget>& targets, SortMode mode, std::vector<std::string> const& sortingIndex, Viewpoint const& from) {
    std::map<std::string, std::size_t, std::less<>> manual;
    for (std::size_t i = 0; i < sortingIndex.size(); ++i) manual.try_emplace(sortingIndex[i], i);
    auto manualKey = [&](WarpTarget const& t) {
        auto it = manual.find(t.id);
        return it == manual.end() ? manual.size() + sortingIndex.size() : it->second;
    };
    std::stable_sort(targets.begin(), targets.end(), [&](WarpTarget const& a, WarpTarget const& b) {
        bool pa = isPinned(a), pb = isPinned(b);
        if (pa != pb) return pa;
        if (pa) return false; // pinned keep their order
        switch (mode) {
        case SortMode::Name:
            return std::lexicographical_compare(a.name.begin(), a.name.end(), b.name.begin(), b.name.end(), [](char x, char y) {
                auto lx = x >= 'A' && x <= 'Z' ? static_cast<char>(x - 'A' + 'a') : x;
                auto ly = y >= 'A' && y <= 'Z' ? static_cast<char>(y - 'A' + 'a') : y;
                return static_cast<unsigned char>(lx) < static_cast<unsigned char>(ly);
            });
        case SortMode::Distance: {
            // Players (Twinbound partners) have no fixed position: they sort with other dimensions.
            bool sa = a.pos.dim == from.dim && a.kind != "player", sb = b.pos.dim == from.dim && b.kind != "player";
            if (sa != sb) return sa;
            if (!sa && a.pos.dim != b.pos.dim) return a.pos.dim < b.pos.dim;
            return distanceTo(from, a.pos) < distanceTo(from, b.pos);
        }
        case SortMode::Manual:
        default:
            return manualKey(a) < manualKey(b);
        }
    });
}

bool matchesSearch(std::string_view name, std::string_view query) {
    if (query.empty()) return true;
    return toLower(name).find(toLower(query)) != std::string::npos;
}

std::string formatDistance(double meters, bool longName) {
    char buf[32];
    if (meters >= 10000 || longName) {
        std::snprintf(buf, sizeof(buf), "%.1fkm", meters / 1000.0);
    } else {
        std::snprintf(buf, sizeof(buf), "%dm", static_cast<int>(meters));
    }
    return buf;
}

std::string formatCooldown(double seconds) {
    long long s = seconds <= 0 ? 0 : static_cast<long long>(std::ceil(seconds));
    char      buf[32];
    std::snprintf(buf, sizeof(buf), "%02lld:%02lld", s / 60, s % 60);
    return buf;
}

bool isSelectable(WarpTarget const& t, bool creative) { return !t.current && (t.canAfford || creative); }

LevelSprite levelSprite(int levels) {
    if (levels <= 0) return {};
    return {std::min(levels, 3), levels > 3};
}

std::string labelColor(WarpTarget const& t, std::vector<GroupInfo> const& groups) {
    if (t.kind == types::kWarpPortal) return "light_purple";
    for (auto const& g : groups) {
        if (g.hidden || g.color.empty()) continue;
        if (std::find(t.groups.begin(), t.groups.end(), g.id) != t.groups.end()) return g.color;
    }
    return "white";
}

std::vector<GroupInfo> filterGroups(std::vector<GroupInfo> const& groups, std::vector<WarpTarget> const& targets, int maxButtons) {
    std::vector<GroupInfo> out;
    for (auto const& g : groups) {
        if (static_cast<int>(out.size()) >= maxButtons) break;
        if (g.hidden) continue;
        bool used = std::any_of(targets.begin(), targets.end(), [&](WarpTarget const& t) {
            return std::find(t.groups.begin(), t.groups.end(), g.id) != t.groups.end();
        });
        if (used) out.push_back(g);
    }
    return out;
}

int panelHeight(int rowCount, int screenHeight) {
    int needed = kHeaderHeight + std::max(0, rowCount) * kRowHeight;
    int h      = std::max(kMinHeight, needed);
    return std::min(h, screenHeight - 40);
}

int maxGroupButtons(int panelHeight) { return 5 + std::max(0, panelHeight - kMinHeight) / kRowHeight; }

} // namespace bs::core::selection
