#include "core/Waystone.h"

#include <algorithm>
#include <cstdio>
#include <random>

namespace bs::core {

std::string GlobalPos::key() const {
    return std::to_string(dim) + ":" + std::to_string(x) + ":" + std::to_string(y) + ":" + std::to_string(z);
}

namespace {
constexpr std::array<std::string_view, 5> kOrigins{"unknown", "wilderness", "dungeon", "village", "player"};
constexpr std::array<std::string_view, 5> kVisibilities{"activation", "global", "team", "shard_only", "sharestone"};

nlohmann::json posJson(GlobalPos const& p) { return {{"dim", p.dim}, {"x", p.x}, {"y", p.y}, {"z", p.z}}; }
GlobalPos      posFrom(nlohmann::json const& j) {
    GlobalPos p;
    if (!j.is_object()) return p;
    p.dim = j.value("dim", 0);
    p.x   = j.value("x", 0);
    p.y   = j.value("y", 0);
    p.z   = j.value("z", 0);
    return p;
}

template <class T>
T valueOr(nlohmann::json const& j, char const* key, T def) {
    auto it = j.find(key);
    if (it == j.end() || it->is_null()) return def;
    try {
        return it->get<T>();
    } catch (...) {
        return def;
    }
}
} // namespace

std::string_view toString(Origin o) { return kOrigins[static_cast<std::size_t>(o)]; }

Origin originFromString(std::string_view s) {
    for (std::size_t i = 0; i < kOrigins.size(); ++i) {
        if (kOrigins[i] == s) return static_cast<Origin>(i);
    }
    return Origin::Unknown;
}

std::string_view toString(Visibility v) { return kVisibilities[static_cast<std::size_t>(v)]; }

std::optional<Visibility> visibilityFromString(std::string_view s) {
    for (std::size_t i = 0; i < kVisibilities.size(); ++i) {
        if (kVisibilities[i] == s) return static_cast<Visibility>(i);
    }
    return std::nullopt;
}

std::optional<std::size_t> dyeIndex(std::string_view color) {
    for (std::size_t i = 0; i < kDyeColors.size(); ++i) {
        if (kDyeColors[i] == color) return i;
    }
    return std::nullopt;
}

namespace types {
constexpr std::string_view kSharestoneSuffix = "_sharestone";

bool isSharestone(std::string_view type) { return sharestoneColor(type).has_value(); }

std::string sharestone(std::string_view color) { return std::string(color) + std::string(kSharestoneSuffix); }

std::optional<std::string> sharestoneColor(std::string_view type) {
    if (!type.ends_with(kSharestoneSuffix)) return std::nullopt;
    auto color = type.substr(0, type.size() - kSharestoneSuffix.size());
    auto idx   = dyeIndex(color);
    if (!idx || *idx == 0) return std::nullopt; // no white sharestone
    return std::string(color);
}
} // namespace types

// ---- WaystoneRecord ----------------------------------------------------------------------------

nlohmann::json WaystoneRecord::toJson() const {
    return {
        {"uid", uid},
        {"type", type},
        {"name", name},
        {"pos", posJson(pos)},
        {"style", style},
        {"facing", facing},
        {"transient", transient},
        {"origin", std::string(toString(origin))},
        {"ownerUid", ownerUid},
        {"ownerName", ownerName},
        {"visibility", std::string(toString(visibility))},
        {"extra", extra},
    };
}

WaystoneRecord WaystoneRecord::fromJson(nlohmann::json const& j) {
    WaystoneRecord r;
    if (!j.is_object()) return r;
    r.uid        = valueOr<std::string>(j, "uid", "");
    r.type       = valueOr<std::string>(j, "type", std::string(types::kWaystone));
    r.name       = valueOr<std::string>(j, "name", "");
    r.pos        = posFrom(j.value("pos", nlohmann::json::object()));
    r.style      = valueOr<std::string>(j, "style", "");
    r.facing     = std::clamp(valueOr<int>(j, "facing", 0), 0, 3);
    r.transient  = valueOr<bool>(j, "transient", false);
    r.origin     = originFromString(valueOr<std::string>(j, "origin", "unknown"));
    r.ownerUid   = valueOr<std::string>(j, "ownerUid", "");
    r.ownerName  = valueOr<std::string>(j, "ownerName", "");
    r.visibility = visibilityFromString(valueOr<std::string>(j, "visibility", "activation")).value_or(Visibility::Activation);
    if (auto it = j.find("extra"); it != j.end() && it->is_object()) r.extra = *it;
    return r;
}

// ---- WaystoneGroup -----------------------------------------------------------------------------

nlohmann::json WaystoneGroup::toJson() const {
    return {
        {"id", id},
        {"name", name},
        {"icon", icon},
        {"color", color},
        {"inbuilt", inbuilt},
        {"hidden", hidden},
        {"sortIndex", sortIndex},
    };
}

WaystoneGroup WaystoneGroup::fromJson(nlohmann::json const& j) {
    WaystoneGroup g;
    if (!j.is_object()) return g;
    g.id        = valueOr<std::string>(j, "id", "");
    g.name      = valueOr<std::string>(j, "name", "");
    g.icon      = valueOr<std::string>(j, "icon", "");
    g.color     = valueOr<std::string>(j, "color", "white");
    g.inbuilt   = valueOr<bool>(j, "inbuilt", false);
    g.hidden    = valueOr<bool>(j, "hidden", false);
    g.sortIndex = valueOr<int>(j, "sortIndex", 0);
    return g;
}

// ---- PlayerWaystoneData ------------------------------------------------------------------------

bool PlayerWaystoneData::isActivated(std::string_view uid) const {
    return std::find(activated.begin(), activated.end(), uid) != activated.end();
}

bool PlayerWaystoneData::activate(std::string const& uid) {
    if (uid.empty() || isActivated(uid)) return false;
    activated.push_back(uid);
    if (std::find(sortingIndex.begin(), sortingIndex.end(), uid) == sortingIndex.end()) sortingIndex.push_back(uid);
    return true;
}

bool PlayerWaystoneData::forget(std::string const& uid) {
    auto before = activated.size();
    std::erase(activated, uid);
    std::erase(sortingIndex, uid);
    return activated.size() != before;
}

std::string PlayerWaystoneData::effectiveName(std::string const& uid, std::string const& name) const {
    if (auto it = aliases.find(uid); it != aliases.end() && !it->second.empty()) return it->second;
    return name;
}

nlohmann::json PlayerWaystoneData::toJson() const {
    nlohmann::json groupsJson = nlohmann::json::object();
    for (auto const& [uid, ids] : groups) groupsJson[uid] = ids;
    nlohmann::json registry = nlohmann::json::array();
    for (auto const& g : groupRegistry) registry.push_back(g.toJson());
    return {
        {"activated", activated},
        {"sortingIndex", sortingIndex},
        {"sortMode", sortMode},
        {"cooldowns", cooldowns},
        {"aliases", aliases},
        {"groups", groupsJson},
        {"hidden", hidden},
        {"groupRegistry", registry},
        {"extra", extra},
    };
}

PlayerWaystoneData PlayerWaystoneData::fromJson(nlohmann::json const& j) {
    PlayerWaystoneData d;
    if (!j.is_object()) return d;
    d.activated    = valueOr<std::vector<std::string>>(j, "activated", {});
    d.sortingIndex = valueOr<std::vector<std::string>>(j, "sortingIndex", {});
    d.sortMode     = valueOr<std::string>(j, "sortMode", "manual");
    d.cooldowns    = valueOr<std::map<std::string, std::int64_t>>(j, "cooldowns", {});
    d.aliases      = valueOr<std::map<std::string, std::string>>(j, "aliases", {});
    d.groups       = valueOr<std::map<std::string, std::vector<std::string>>>(j, "groups", {});
    d.hidden       = valueOr<std::set<std::string>>(j, "hidden", {});
    if (auto it = j.find("groupRegistry"); it != j.end() && it->is_array()) {
        for (auto const& g : *it) d.groupRegistry.push_back(WaystoneGroup::fromJson(g));
    }
    if (auto it = j.find("extra"); it != j.end() && it->is_object()) d.extra = *it;
    return d;
}

// ---- misc --------------------------------------------------------------------------------------

std::string randomUuid() {
    static thread_local std::mt19937_64 rng{std::random_device{}() ^ (static_cast<std::uint64_t>(std::random_device{}()) << 32)};
    std::uint64_t hi = rng(), lo = rng();
    hi = (hi & 0xFFFFFFFFFFFF0FFFull) | 0x0000000000004000ull; // version 4
    lo = (lo & 0x3FFFFFFFFFFFFFFFull) | 0x8000000000000000ull; // variant 10xx
    char buf[37];
    std::snprintf(
        buf,
        sizeof buf,
        "%08x-%04x-%04x-%04x-%012llx",
        static_cast<unsigned>(hi >> 32),
        static_cast<unsigned>((hi >> 16) & 0xFFFF),
        static_cast<unsigned>(hi & 0xFFFF),
        static_cast<unsigned>(lo >> 48),
        static_cast<unsigned long long>(lo & 0xFFFFFFFFFFFFull)
    );
    return buf;
}

bool globMatch(std::string_view pattern, std::string_view text) {
    std::size_t p = 0, t = 0, star = std::string_view::npos, mark = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p, ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            mark = t;
        } else if (star != std::string_view::npos) {
            p = star + 1;
            t = ++mark;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

} // namespace bs::core
