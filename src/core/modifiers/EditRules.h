#pragma once

// Server-side save rules of the Edit Waystone screen (docs/SPEC.md §3.4), pure logic.

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Waystone.h"

namespace bs::core::edit {

inline constexpr std::size_t      kMaxNameLength = 128; ///< code points (the name field's limit)
inline constexpr std::string_view kUntitled      = "Untitled Waystone";
inline constexpr double           kMaxEditDistance = 4.0;

/// Clips `name` to `kMaxNameLength` code points (UTF-8) and strips control characters.
[[nodiscard]] std::string clipName(std::string_view name);

/// The name stored on save: an empty (blank) name becomes "Untitled Waystone"; a name equal to the
/// inventory button's named target gets "*" appended when `nameTaken(name)` (a waystone with that
/// name exists). `inventoryButton` is the config value ("", NONE, NEAREST, ANY or a name).
[[nodiscard]] std::string finalName(
    std::string_view                              requested,
    std::string_view                              inventoryButton,
    std::function<bool(std::string const&)> const& nameTaken
);

/// The requested visibility when it is one of `options`, otherwise the first option (nullopt when
/// there are none).
[[nodiscard]] std::optional<Visibility> pickVisibility(std::string_view requested, std::vector<Visibility> const& options);

/// The inventory button mode names a specific waystone.
[[nodiscard]] bool isNamedInventoryTarget(std::string_view inventoryButton);

} // namespace bs::core::edit
