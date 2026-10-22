#include "core/groups/GroupRegistry.h"

#include <algorithm>
#include <cctype>

namespace bs::core::groups {

namespace {

struct InbuiltDef {
    std::string_view id;
    std::string_view name;
    std::string_view key;
    std::string_view icon;
    std::string_view color;
    int              sortIndex;
};

// Waystones 21.1.46's inbuilt groups (docs/SPEC.md §4.4). Dimension groups sort after the others.
constexpr std::array<InbuiltDef, 7> kInbuilt{{
    {kFavorites, "Favorites", "waystones.groups.favorites", "favorites", "gold", -30},
    {kPlayers, "Players", "waystones.groups.players", "players", "green", -20},
    {kGlobal, "Global Waystones", "waystones.groups.global", "global", "yellow", -10},
    {kTeams, "Teams", "waystones.groups.teams", "teams", "aqua", -5},
    {"waystones:dimension/0", "Overworld", "waystones.groups.dimension.overworld", "overworld", "white", 0},
    {"waystones:dimension/1", "The Nether", "waystones.groups.dimension.the_nether", "the_nether", "white", 0},
    {"waystones:dimension/2", "The End", "waystones.groups.dimension.the_end", "the_end", "white", 0},
}};

InbuiltDef const* inbuiltDef(std::string_view id) {
    for (auto const& d : kInbuilt) {
        if (d.id == id) return &d;
    }
    return nullptr;
}

template <std::size_t N>
std::string cycle(std::array<std::string_view, N> const& list, std::string_view current, int direction) {
    auto it = std::find(list.begin(), list.end(), current);
    if (it == list.end()) return std::string(list.front());
    auto n   = static_cast<int>(N);
    auto idx = (static_cast<int>(it - list.begin()) + (direction < 0 ? -1 : 1) + n) % n;
    return std::string(list[static_cast<std::size_t>(idx)]);
}

/// The registry entry of `id`; inbuilt groups get an override entry on demand.
WaystoneGroup* entryFor(std::vector<WaystoneGroup>& registry, std::string_view id, bool create) {
    for (auto& g : registry) {
        if (g.id == id) return &g;
    }
    if (!create) return nullptr;
    auto const* def = inbuiltDef(id);
    if (!def) return nullptr;
    WaystoneGroup g;
    g.id        = std::string(def->id);
    g.icon      = std::string(def->icon);
    g.color     = std::string(def->color);
    g.inbuilt   = true;
    g.sortIndex = def->sortIndex;
    registry.push_back(std::move(g));
    return &registry.back();
}

/// Writes `order` (group ids) back as sort indices 0..n-1.
void renumber(std::vector<WaystoneGroup>& registry, std::vector<std::string> const& order) {
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (auto* e = entryFor(registry, order[i], true)) e->sortIndex = static_cast<int>(i);
    }
}

std::vector<std::string> orderOf(std::vector<WaystoneGroup> const& registry) {
    std::vector<std::string> ids;
    for (auto const& g : resolve(registry)) ids.push_back(g.id);
    return ids;
}

} // namespace

std::string dimensionGroup(int dim) { return std::string(kDimensionPrefix) + std::to_string(dim); }

bool isUserGroup(std::string_view id) { return id.starts_with(kUserPrefix) && id.size() > kUserPrefix.size(); }
bool isInbuilt(std::string_view id) { return inbuiltDef(id) != nullptr; }
bool isIcon(std::string_view icon) { return std::find(kIcons.begin(), kIcons.end(), icon) != kIcons.end(); }
bool isColor(std::string_view color) { return std::find(kColors.begin(), kColors.end(), color) != kColors.end(); }

std::vector<WaystoneGroup> inbuiltDefaults() {
    std::vector<WaystoneGroup> out;
    for (auto const& d : kInbuilt) {
        out.push_back({std::string(d.id), std::string(d.name), std::string(d.icon), std::string(d.color), true, false, d.sortIndex});
    }
    return out;
}

std::string nameKey(std::string_view id) {
    if (auto const* d = inbuiltDef(id)) return std::string(d->key);
    return {};
}
std::string iconKey(std::string_view icon) { return "gui.waystones.group_settings.icon." + std::string(icon); }
std::string colorKey(std::string_view color) { return "color.waystones.group." + std::string(color); }

std::string displayName(std::string_view id) {
    std::string out;
    bool        upper = true;
    for (char c : id) {
        if (c == '_') {
            out += ' ';
            upper = true;
            continue;
        }
        out += upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
        upper = false;
    }
    return out;
}

std::vector<WaystoneGroup> resolve(std::vector<WaystoneGroup> const& registry, Translator const& tr) {
    std::vector<WaystoneGroup> out;
    for (auto const& d : kInbuilt) {
        WaystoneGroup g{std::string(d.id), std::string(d.name), std::string(d.icon), std::string(d.color), true, false, d.sortIndex};
        for (auto const& o : registry) {
            if (o.id != d.id) continue;
            if (!o.name.empty()) g.name = o.name;
            if (isIcon(o.icon)) g.icon = o.icon;
            if (isColor(o.color)) g.color = o.color;
            g.hidden    = o.hidden;
            g.sortIndex = o.sortIndex;
            break;
        }
        out.push_back(std::move(g));
    }
    for (auto const& g : registry) {
        if (!isUserGroup(g.id)) continue;
        if (std::any_of(out.begin(), out.end(), [&](auto const& x) { return x.id == g.id; })) continue;
        auto u    = g;
        u.inbuilt = false;
        u.hidden  = false;
        if (!isIcon(u.icon)) u.icon = std::string(kIcons.front());
        if (!isColor(u.color)) u.color = std::string(kColors.front());
        out.push_back(std::move(u));
    }
    // Sorted by sort index, then name (the IGroups contract). Ties use the untranslated names so
    // the order is the same in every locale (the server reorders by this order).
    std::stable_sort(out.begin(), out.end(), [](auto const& a, auto const& b) {
        return a.sortIndex != b.sortIndex ? a.sortIndex < b.sortIndex : a.name < b.name;
    });
    if (tr) {
        for (auto& g : out) {
            auto const* d = inbuiltDef(g.id);
            if (d && g.name == d->name) {
                bool overridden = std::any_of(registry.begin(), registry.end(), [&](auto const& o) { return o.id == g.id && !o.name.empty(); });
                if (!overridden) g.name = tr(std::string(d->key), std::string(d->name));
            }
        }
    }
    return out;
}

std::optional<WaystoneGroup> find(std::vector<WaystoneGroup> const& registry, std::string_view id, Translator const& tr) {
    for (auto& g : resolve(registry, tr)) {
        if (g.id == id) return g;
    }
    return std::nullopt;
}

std::string sanitizeName(std::string_view name) {
    auto b = name.find_first_not_of(" \t\r\n");
    if (b == std::string_view::npos) return {};
    auto e = name.find_last_not_of(" \t\r\n");
    name   = name.substr(b, e - b + 1);
    std::string out;
    std::size_t cps = 0;
    for (std::size_t i = 0; i < name.size();) {
        // One code point: lead byte + continuation bytes.
        std::size_t len = 1;
        while (i + len < name.size() && (static_cast<unsigned char>(name[i + len]) & 0xC0) == 0x80) ++len;
        auto cp = name.substr(i, len);
        i += len;
        // Control characters and formatting codes (§) are dropped; they do not count.
        if (len == 1 && (static_cast<unsigned char>(cp[0]) < 0x20 || cp[0] == 0x7F)) continue;
        if (cp == "\xC2\xA7") continue;
        if (cps == kMaxNameLength) break;
        ++cps;
        out += cp;
    }
    return out;
}

std::string cycleIcon(std::string_view icon, int direction) { return cycle(kIcons, icon, direction); }
std::string cycleColor(std::string_view color, int direction) { return cycle(kColors, color, direction); }

std::string
create(std::vector<WaystoneGroup>& registry, std::string_view name, std::string_view icon, std::string_view color, std::string const& uuid) {
    if (static_cast<std::size_t>(std::count_if(registry.begin(), registry.end(), [](auto const& g) { return isUserGroup(g.id); }))
        >= kMaxUserGroups) {
        return {};
    }
    int last = 0;
    for (auto const& g : resolve(registry)) last = std::max(last, g.sortIndex);
    WaystoneGroup g;
    g.id        = std::string(kUserPrefix) + uuid;
    g.name      = sanitizeName(name);
    g.icon      = isIcon(icon) ? std::string(icon) : std::string(kIcons.front());
    g.color     = isColor(color) ? std::string(color) : std::string(kColors.front());
    g.sortIndex = last + 1;
    registry.push_back(g);
    return g.id;
}

bool update(
    std::vector<WaystoneGroup>& registry,
    std::string_view            id,
    std::string_view            name,
    std::string_view            icon,
    std::string_view            color,
    std::string_view            defaultName
) {
    if (!isIcon(icon) || !isColor(color)) return false;
    if (!isInbuilt(id) && !(isUserGroup(id) && entryFor(registry, id, false))) return false;
    auto* e = entryFor(registry, id, true);
    if (!e) return false;
    auto n = sanitizeName(name);
    if (e->inbuilt && (n == defaultName || n.empty())) n.clear();
    e->name  = std::move(n);
    e->icon  = std::string(icon);
    e->color = std::string(color);
    return true;
}

bool remove(std::vector<WaystoneGroup>& registry, std::map<std::string, std::vector<std::string>>& assignments, std::string_view id) {
    if (!isUserGroup(id)) return false;
    auto before = registry.size();
    std::erase_if(registry, [&](auto const& g) { return g.id == id; });
    if (registry.size() == before) return false;
    for (auto it = assignments.begin(); it != assignments.end();) {
        std::erase(it->second, std::string(id));
        it = it->second.empty() ? assignments.erase(it) : std::next(it);
    }
    return true;
}

bool setHidden(std::vector<WaystoneGroup>& registry, std::string_view id, bool hidden) {
    if (!isInbuilt(id)) return false;
    entryFor(registry, id, true)->hidden = hidden;
    return true;
}

bool swap(std::vector<WaystoneGroup>& registry, std::string_view a, std::string_view b) {
    auto order = orderOf(registry);
    auto ia    = std::find(order.begin(), order.end(), a);
    auto ib    = std::find(order.begin(), order.end(), b);
    if (ia == order.end() || ib == order.end() || ia == ib) return false;
    std::iter_swap(ia, ib);
    renumber(registry, order);
    return true;
}

bool moveTo(std::vector<WaystoneGroup>& registry, std::string_view id, int index) {
    auto order = orderOf(registry);
    auto it    = std::find(order.begin(), order.end(), id);
    if (it == order.end()) return false;
    auto moved = *it;
    order.erase(it);
    auto pos = static_cast<std::size_t>(std::clamp(index, 0, static_cast<int>(order.size())));
    order.insert(order.begin() + static_cast<std::ptrdiff_t>(pos), moved);
    renumber(registry, order);
    return true;
}

std::vector<std::string> implicitGroups(std::string_view kind, bool global, bool team, int dim) {
    std::vector<std::string> out;
    if (global) out.emplace_back(kGlobal);
    if (kind == "player") out.emplace_back(kPlayers);
    if (team) out.emplace_back(kTeams);
    if (kind != "player") out.push_back(dimensionGroup(dim));
    return out;
}

std::vector<std::string>
groupsOf(std::vector<WaystoneGroup> const& registry, std::vector<std::string> const& assigned, std::vector<std::string> const& implicit) {
    std::vector<std::string> out;
    auto add = [&](std::string const& id) {
        if (std::find(out.begin(), out.end(), id) == out.end()) out.push_back(id);
    };
    for (auto const& id : assigned) {
        bool exists = isInbuilt(id) || std::any_of(registry.begin(), registry.end(), [&](auto const& g) { return g.id == id; });
        if (exists) add(id);
    }
    for (auto const& id : implicit) add(id);
    return out;
}

} // namespace bs::core::groups
