#include "core/visuals/Visuals.h"

#include <cmath>

namespace bs::core::visuals {

bool isRendered(std::string_view type) { return type == types::kWaystone || types::isSharestone(type); }

bool inRange(GlobalPos const& pos, int dim, double x, double y, double z, double range) {
    if (pos.dim != dim) return false;
    double dx = pos.x + 0.5 - x, dy = pos.y + 0.5 - y, dz = pos.z + 0.5 - z;
    return dx * dx + dy * dy + dz * dz <= range * range;
}

std::uint32_t waystoneRuneColor(std::string_view style) {
    if (style == "blackstone_waystone") return kRuneBlackstone;
    if (style == "end_stone_waystone") return kRuneEndStone;
    return kRuneWhite;
}

std::uint32_t dyeRuneColor(std::string_view typeOrColor) {
    auto color = typeOrColor;
    for (std::string_view suffix : {std::string_view("_sharestone"), std::string_view("_portstone")}) {
        if (color.ends_with(suffix)) {
            color.remove_suffix(suffix.size());
            break;
        }
    }
    auto idx = dyeIndex(color);
    return idx ? kDyeDiffuse[*idx] : kRuneWhite;
}

std::uint32_t runeColor(Entry const& e) {
    if (types::isSharestone(e.type)) return dyeRuneColor(e.type);
    return waystoneRuneColor(e.style);
}

bool showsRunes(Entry const& e) { return types::isSharestone(e.type) || (e.type == types::kWaystone && e.activated); }

bool hasAmbientParticles(Entry const& e) { return e.type == types::kWaystone && e.activated; }

bool hasFloatingItem(Entry const& e) { return types::isSharestone(e.type); }

nlohmann::json toJson(std::vector<Entry> const& entries) {
    auto list = nlohmann::json::array();
    for (auto const& e : entries) {
        list.push_back({
            {"uid",       e.uid      },
            {"dim",       e.pos.dim  },
            {"x",         e.pos.x    },
            {"y",         e.pos.y    },
            {"z",         e.pos.z    },
            {"type",      e.type     },
            {"style",     e.style    },
            {"facing",    e.facing   },
            {"activated", e.activated},
        });
    }
    return {
        {"entries", std::move(list)},
    };
}

std::vector<Entry> fromJson(nlohmann::json const& payload) {
    std::vector<Entry> out;
    if (!payload.is_object()) return out;
    auto it = payload.find("entries");
    if (it == payload.end() || !it->is_array()) return out;
    for (auto const& j : *it) {
        if (!j.is_object()) continue;
        try {
            Entry e;
            e.uid       = j.value("uid", std::string{});
            e.pos       = {j.value("dim", 0), j.value("x", 0), j.value("y", 0), j.value("z", 0)};
            e.type      = j.value("type", std::string{});
            e.style     = j.value("style", std::string{});
            e.facing    = j.value("facing", 0) & 3;
            e.activated = j.value("activated", false);
            if (!e.uid.empty() && isRendered(e.type)) out.push_back(std::move(e));
        } catch (nlohmann::json::exception const&) {
            // wrong field types: skip the entry
        }
    }
    return out;
}

std::array<RuneQuad, 4> runeQuads(GlobalPos const& pos) {
    double cx = pos.x + 0.5, cy = pos.y + kRuneCentreY, cz = pos.z + 0.5;
    // Java 2D facing order: south (+z), west (-x), north (-z), east (+x).
    constexpr std::array<std::array<double, 2>, 4> kNormals{
        {{0, 1}, {-1, 0}, {0, -1}, {1, 0}}
    };
    std::array<RuneQuad, 4> out{};
    for (std::size_t i = 0; i < 4; ++i) {
        auto [nx, nz] = kNormals[i];
        out[i]        = {cx + nx * kRuneInset, cy, cz + nz * kRuneInset, nx, nz};
    }
    return out;
}

FloatPose floatingPose(std::uint64_t gameTime, double partialTick) {
    double spin = static_cast<double>(gameTime % 720) + partialTick;
    double t    = static_cast<double>(gameTime) + partialTick;
    return {spin / 2.0, std::sin(t / 8.0) * 0.025};
}

} // namespace bs::core::visuals
