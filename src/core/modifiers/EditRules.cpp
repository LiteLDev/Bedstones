#include "core/modifiers/EditRules.h"

#include <algorithm>
#include <cctype>

namespace bs::core::edit {

std::string clipName(std::string_view name) {
    std::string out;
    std::size_t points = 0;
    for (std::size_t i = 0; i < name.size();) {
        auto        c   = static_cast<unsigned char>(name[i]);
        std::size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
        if (i + len > name.size()) break;
        if (points == kMaxNameLength) break;
        if (!(len == 1 && (c < 0x20 || c == 0x7F))) {
            out.append(name.substr(i, len));
            ++points;
        }
        i += len;
    }
    return out;
}

bool isNamedInventoryTarget(std::string_view mode) {
    if (mode.empty()) return false;
    std::string upper(mode);
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return upper != "NONE" && upper != "NEAREST" && upper != "ANY";
}

std::string finalName(std::string_view requested, std::string_view inventoryButton, std::function<bool(std::string const&)> const& nameTaken) {
    auto name  = clipName(requested);
    bool blank = std::all_of(name.begin(), name.end(), [](unsigned char c) { return std::isspace(c); });
    if (blank) return std::string(kUntitled);
    if (isNamedInventoryTarget(inventoryButton) && name == inventoryButton && nameTaken && nameTaken(name)) name += "*";
    return name;
}

std::optional<Visibility> pickVisibility(std::string_view requested, std::vector<Visibility> const& options) {
    if (options.empty()) return std::nullopt;
    if (auto v = visibilityFromString(requested)) {
        if (std::find(options.begin(), options.end(), *v) != options.end()) return v;
    }
    return options.front();
}

} // namespace bs::core::edit
