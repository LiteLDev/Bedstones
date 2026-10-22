#include "core/feather/Feather.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace bs::core::feather {

std::optional<double> rayHitPlayer(Vec eye, Vec look, Vec feet, double range) {
    double len = std::sqrt(look.x * look.x + look.y * look.y + look.z * look.z);
    if (len <= 1e-9 || range <= 0) return std::nullopt;
    double const dir[3] = {look.x / len, look.y / len, look.z / len};
    double const org[3] = {eye.x, eye.y, eye.z};
    double const half   = kPlayerWidth / 2 + kPickInflate;
    double const lo[3]  = {feet.x - half, feet.y - kPickInflate, feet.z - half};
    double const hi[3]  = {feet.x + half, feet.y + kPlayerHeight + kPickInflate, feet.z + half};
    // Slab test of the segment [0, range].
    double tMin = 0, tMax = range;
    for (int i = 0; i < 3; ++i) {
        if (std::abs(dir[i]) < 1e-12) {
            if (org[i] < lo[i] || org[i] > hi[i]) return std::nullopt;
            continue;
        }
        double t1 = (lo[i] - org[i]) / dir[i];
        double t2 = (hi[i] - org[i]) / dir[i];
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return std::nullopt;
    }
    return tMin;
}

int pickTarget(Vec eye, Vec look, std::vector<Vec> const& candidateFeet, double range) {
    int    best  = -1;
    double bestT = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < candidateFeet.size(); ++i) {
        if (auto t = rayHitPlayer(eye, look, candidateFeet[i], range); t && *t < bestT) {
            bestT = *t;
            best  = static_cast<int>(i);
        }
    }
    return best;
}

Vec heartPos(Vec a, Vec b) { return {(a.x + b.x) / 2, (a.y + b.y) / 2 + 1.1, (a.z + b.z) / 2}; }

std::vector<std::string> parseLinks(std::string_view links) {
    std::vector<std::string> out;
    std::size_t              start = 0;
    while (start <= links.size()) {
        auto end = links.find(',', start);
        if (end == std::string_view::npos) end = links.size();
        auto part = links.substr(start, end - start);
        while (!part.empty() && part.front() == ' ') part.remove_prefix(1);
        while (!part.empty() && part.back() == ' ') part.remove_suffix(1);
        if (!part.empty() && std::find(out.begin(), out.end(), part) == out.end()) out.emplace_back(part);
        start = end + 1;
    }
    return out;
}

std::string formatLinks(std::vector<std::string> const& links) {
    std::string out;
    for (auto const& l : links) {
        if (l.empty()) continue;
        if (!out.empty()) out += ',';
        out += l;
    }
    return out;
}

bool hasLink(std::string_view links, std::string_view featherId) {
    if (featherId.empty()) return false;
    auto all = parseLinks(links);
    return std::find(all.begin(), all.end(), featherId) != all.end();
}

std::string withLink(std::string_view links, std::string_view featherId) {
    auto all = parseLinks(links);
    if (!featherId.empty() && std::find(all.begin(), all.end(), featherId) == all.end()) all.emplace_back(featherId);
    return formatLinks(all);
}

bool areLinked(FeatherInfo const& a, FeatherInfo const& b) {
    return !a.id.empty() && !b.id.empty() && a.id != b.id && hasLink(a.links, b.id) && hasLink(b.links, a.id);
}

bool anyLinked(std::vector<FeatherInfo> const& a, std::vector<FeatherInfo> const& b) {
    for (auto const& x : a) {
        for (auto const& y : b) {
            if (areLinked(x, y)) return true;
        }
    }
    return false;
}

int TwinningTracker::advance(std::string const& player, std::string const& target) {
    auto& p = mProgress[player];
    if (p.target != target) {
        p.target = target;
        p.ticks  = 0;
    }
    return ++p.ticks;
}

void TwinningTracker::reset(std::string const& player) { mProgress.erase(player); }

void TwinningTracker::retain(std::vector<std::string> const& active) {
    std::erase_if(mProgress, [&](auto const& kv) { return std::find(active.begin(), active.end(), kv.first) == active.end(); });
}

int TwinningTracker::ticks(std::string const& player) const {
    auto it = mProgress.find(player);
    return it == mProgress.end() ? 0 : it->second.ticks;
}

} // namespace bs::core::feather
