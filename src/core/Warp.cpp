#include "core/Warp.h"

#include <array>
#include <cmath>

namespace bs::core {

namespace {
constexpr std::array<std::string_view, 13> kSources{
    "waystone",
    "sharestone",
    "portstone",
    "warp_plate",
    "inventory_button",
    "warp_scroll",
    "bound_scroll",
    "return_scroll",
    "portal_scroll",
    "warp_stone",
    "warp_portal",
    "admin",
    "command",
};

nlohmann::json posJson(GlobalPos const& p) { return {{"dim", p.dim}, {"x", p.x}, {"y", p.y}, {"z", p.z}}; }
GlobalPos      posFrom(nlohmann::json const& j) {
    if (!j.is_object()) return {};
    return {j.value("dim", 0), j.value("x", 0), j.value("y", 0), j.value("z", 0)};
}
} // namespace

std::string_view toString(WarpSourceKind k) { return kSources[static_cast<std::size_t>(k)]; }

std::optional<WarpSourceKind> warpSourceFromString(std::string_view s) {
    for (std::size_t i = 0; i < kSources.size(); ++i) {
        if (kSources[i] == s) return static_cast<WarpSourceKind>(i);
    }
    return std::nullopt;
}

bool WarpSource::isBlock() const {
    switch (kind) {
    case WarpSourceKind::Waystone:
    case WarpSourceKind::Sharestone:
    case WarpSourceKind::Portstone:
    case WarpSourceKind::WarpPlate:
    case WarpSourceKind::WarpPortal:
        return true;
    default:
        return false;
    }
}

bool WarpSource::isItem() const {
    switch (kind) {
    case WarpSourceKind::WarpScroll:
    case WarpSourceKind::BoundScroll:
    case WarpSourceKind::ReturnScroll:
    case WarpSourceKind::PortalScroll:
    case WarpSourceKind::WarpStone:
        return true;
    default:
        return false;
    }
}

nlohmann::json WarpSource::toJson() const {
    return {
        {"kind", std::string(toString(kind))},
        {"fromUid", fromUid},
        {"fromPos", posJson(fromPos)},
        {"itemId", itemId},
        {"portstoneColor", portstoneColor},
        {"hand", hand},
    };
}

WarpSource WarpSource::fromJson(nlohmann::json const& j) {
    WarpSource s;
    if (!j.is_object()) return s;
    s.kind           = warpSourceFromString(j.value("kind", std::string{})).value_or(WarpSourceKind::Command);
    s.fromUid        = j.value("fromUid", std::string{});
    s.fromPos        = posFrom(j.value("fromPos", nlohmann::json::object()));
    s.itemId         = j.value("itemId", std::string{});
    s.portstoneColor = j.value("portstoneColor", std::string{});
    s.hand           = j.value("hand", 0);
    return s;
}

bool Requirement::free() const {
    return xpPoints <= 0 && levels <= 0 && items.empty() && activeCooldown <= 0 && refuse.empty();
}

nlohmann::json Requirement::toJson() const {
    nlohmann::json items_ = nlohmann::json::array();
    for (auto const& i : items) items_.push_back({{"id", i.id}, {"count", i.count}});
    nlohmann::json cds = nlohmann::json::array();
    for (auto const& c : cooldowns) cds.push_back({{"id", c.id}, {"seconds", c.seconds}, {"soft", c.soft}});
    return {
        {"xpPoints", xpPoints},
        {"levels", levels},
        {"items", items_},
        {"cooldowns", cds},
        {"activeCooldown", activeCooldown},
        {"refuse", refuse},
        {"dismount", dismount},
    };
}

Requirement Requirement::fromJson(nlohmann::json const& j) {
    Requirement r;
    if (!j.is_object()) return r;
    r.xpPoints       = j.value("xpPoints", 0);
    r.levels         = j.value("levels", 0);
    r.activeCooldown = j.value("activeCooldown", 0.0);
    r.refuse         = j.value("refuse", std::string{});
    r.dismount       = j.value("dismount", false);
    if (auto it = j.find("items"); it != j.end() && it->is_array()) {
        for (auto const& i : *it) r.items.push_back({i.value("id", std::string{}), i.value("count", 0)});
    }
    if (auto it = j.find("cooldowns"); it != j.end() && it->is_array()) {
        for (auto const& c : *it) r.cooldowns.push_back({c.value("id", std::string{}), c.value("seconds", 0.0), c.value("soft", false)});
    }
    return r;
}

nlohmann::json WarpTarget::toJson() const {
    return {
        {"id", id},
        {"kind", kind},
        {"name", name},
        {"pos", posJson(pos)},
        {"global", global},
        {"current", current},
        {"pinned", pinned},
        {"canAfford", canAfford},
        {"requirement", requirement.toJson()},
        {"groups", groups},
        {"extra", extra},
    };
}

WarpTarget WarpTarget::fromJson(nlohmann::json const& j) {
    WarpTarget t;
    if (!j.is_object()) return t;
    t.id          = j.value("id", std::string{});
    t.kind        = j.value("kind", std::string{});
    t.name        = j.value("name", std::string{});
    t.pos         = posFrom(j.value("pos", nlohmann::json::object()));
    t.global      = j.value("global", false);
    t.current     = j.value("current", false);
    t.pinned      = j.value("pinned", false);
    t.canAfford   = j.value("canAfford", true);
    t.requirement = Requirement::fromJson(j.value("requirement", nlohmann::json::object()));
    if (auto it = j.find("groups"); it != j.end() && it->is_array()) {
        for (auto const& g : *it) {
            if (g.is_string()) t.groups.push_back(g.get<std::string>());
        }
    }
    if (auto it = j.find("extra"); it != j.end() && it->is_object()) t.extra = *it;
    return t;
}

int xpForLevel(int level) {
    if (level >= 30) return 112 + (level - 30) * 9;
    if (level >= 15) return 37 + (level - 15) * 5;
    return 7 + level * 2;
}

int totalXpForLevel(int level) {
    int total = 0;
    for (int i = 0; i < level; ++i) total += xpForLevel(i);
    return total;
}

int levelsForPoints(int level, float progress, int points) {
    if (points <= 0) return 0;
    // Walk down from the player's current total like Java's XP drain would.
    double have   = totalXpForLevel(level) + progress * xpForLevel(level);
    double after  = have - points;
    int    target = level;
    while (target > 0 && totalXpForLevel(target) > after) --target;
    int levels = level - target;
    return levels > 0 ? levels : 1;
}

} // namespace bs::core
