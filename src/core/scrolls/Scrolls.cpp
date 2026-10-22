#include "core/scrolls/Scrolls.h"

#include <algorithm>
#include <limits>

#include "core/Format.h"

namespace bs::core::scrolls {

namespace {
struct Entry {
    ScrollItem       item;
    std::string_view path;
};
constexpr Entry kEntries[]{
    {ScrollItem::WarpStone, "warp_stone"},
    {ScrollItem::WarpScroll, "warp_scroll"},
    {ScrollItem::ReturnScroll, "return_scroll"},
    {ScrollItem::BlankScroll, "blank_scroll"},
    {ScrollItem::BoundScroll, "bound_scroll"},
};
} // namespace

std::optional<ScrollItem> fromPath(std::string_view p) {
    for (auto const& e : kEntries) {
        if (e.path == p) return e.item;
    }
    return std::nullopt;
}

std::string_view path(ScrollItem item) {
    for (auto const& e : kEntries) {
        if (e.item == item) return e.path;
    }
    return {};
}

bool chargeable(ScrollItem item) { return item != ScrollItem::BlankScroll; }

std::optional<WarpSourceKind> sourceKind(ScrollItem item) {
    switch (item) {
    case ScrollItem::WarpStone:
        return WarpSourceKind::WarpStone;
    case ScrollItem::WarpScroll:
        return WarpSourceKind::WarpScroll;
    case ScrollItem::ReturnScroll:
        return WarpSourceKind::ReturnScroll;
    case ScrollItem::BoundScroll:
        return WarpSourceKind::BoundScroll;
    case ScrollItem::BlankScroll:
        return std::nullopt;
    }
    return std::nullopt;
}

bool consumedOnWarp(ScrollItem item) {
    return item == ScrollItem::WarpScroll || item == ScrollItem::ReturnScroll || item == ScrollItem::BoundScroll;
}

int useTime(ScrollItem item, int warpStoneUseTime, int scrollUseTime) {
    return std::max(0, item == ScrollItem::WarpStone ? warpStoneUseTime : scrollUseTime);
}

ChargeParticles chargeParticles(ScrollItem item, int elapsed, int duration) {
    ChargeParticles out;
    if (duration <= 0 || elapsed <= 0 || elapsed > duration) return out;
    int   remaining = duration - elapsed;
    float progress  = static_cast<float>(elapsed) / static_cast<float>(duration);
    int   maxCount  = std::max(4, static_cast<int>(progress * 48));
    if (remaining % 5 == 0) {
        if (item == ScrollItem::WarpStone) {
            out.reversePortal = std::min(4, maxCount); // around the hand
            if (progress >= 0.25f) out.crimsonSpore = maxCount;
            if (progress >= 0.5f) out.reversePortal += maxCount;
            if (progress >= 0.75f) out.witch = maxCount / 3;
        } else {
            out.reversePortal = maxCount;
            if (progress >= 0.25f) out.crimsonSpore = maxCount;
            if (progress >= 0.5f) out.witch = maxCount / 3;
        }
    }
    if (remaining == 1) out.portal = maxCount;
    return out;
}

std::string nearest(std::vector<Candidate> const& activated, int dim, double x, double y, double z) {
    std::string best;
    double      bestDist = std::numeric_limits<double>::max();
    for (auto const& c : activated) {
        if (c.pos.dim != dim) continue;
        double dx = c.pos.x + 0.5 - x, dy = c.pos.y + 0.5 - y, dz = c.pos.z + 0.5 - z;
        double d  = dx * dx + dy * dy + dz * dz;
        if (d < bestDist) {
            bestDist = d;
            best     = c.uid;
        }
    }
    return best;
}

std::string returnTarget(std::string const& returnPortalUid, bool returnPortalExists, std::string const& nearestUid) {
    if (!returnPortalUid.empty() && returnPortalExists) return returnPortalUid;
    return nearestUid;
}

bool canBind(bool inventoryHasRoom, bool creative, int handCount) { return inventoryHasRoom || (!creative && handCount == 1); }

std::string boundLore(std::string_view boundToTemplate, std::string_view name) {
    return "§r§7" + formatMessageArgs(boundToTemplate, "§b" + std::string(name) + "§7");
}

} // namespace bs::core::scrolls
