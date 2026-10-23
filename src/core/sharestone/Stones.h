#pragma once

// Sharestone / portstone block naming (pure, docs/SPEC.md §1.3-1.4).

#include <optional>
#include <string>
#include <string_view>

namespace bs::core::sharestone {

enum class Family { Sharestone, Portstone };

/// One dyed sharestone or portstone block.
struct StoneKind {
    Family      family = Family::Sharestone;
    std::string color; ///< dye color, e.g. "red"

    friend bool operator==(StoneKind const&, StoneKind const&) = default;
};

/// "red_sharestone" -> {Sharestone, "red"}, "white_portstone" -> {Portstone, "white"}. nullopt for
/// anything else (including "white_sharestone", which does not exist).
[[nodiscard]] std::optional<StoneKind> parse(std::string_view path);
/// Block path of a stone ("<color>_sharestone" / "<color>_portstone").
[[nodiscard]] std::string path(StoneKind const& kind);

/// English name of a dye color ("light_blue" -> "Light Blue").
[[nodiscard]] std::string colorName(std::string_view color);
/// English block name ("Red Sharestone", "Blue Portstone"; the white portstone is "Portstone").
[[nodiscard]] std::string displayName(StoneKind const& kind);

} // namespace bs::core::sharestone
