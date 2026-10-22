#include "core/sharestone/Stones.h"

#include <cctype>

#include "core/Waystone.h"

namespace bs::core::sharestone {

namespace {
constexpr std::string_view kSharestone = "_sharestone";
constexpr std::string_view kPortstone  = "_portstone";
} // namespace

std::optional<StoneKind> parse(std::string_view path) {
    if (path.ends_with(kSharestone)) {
        auto color = types::sharestoneColor(path); // rejects white and unknown colors
        if (!color) return std::nullopt;
        return StoneKind{Family::Sharestone, *color};
    }
    if (path.ends_with(kPortstone)) {
        auto color = path.substr(0, path.size() - kPortstone.size());
        if (!dyeIndex(color)) return std::nullopt;
        return StoneKind{Family::Portstone, std::string(color)};
    }
    return std::nullopt;
}

std::string path(StoneKind const& kind) {
    return kind.color + std::string(kind.family == Family::Sharestone ? kSharestone : kPortstone);
}

std::string colorName(std::string_view color) {
    std::string out;
    bool        upper = true;
    for (char c : color) {
        if (c == '_') {
            out   += ' ';
            upper  = true;
            continue;
        }
        out   += upper ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c;
        upper  = false;
    }
    return out;
}

std::string displayName(StoneKind const& kind) {
    if (kind.family == Family::Portstone)
        return kind.color == "white" ? "Portstone" : colorName(kind.color) + " Portstone";
    return colorName(kind.color) + " Sharestone";
}

} // namespace bs::core::sharestone
