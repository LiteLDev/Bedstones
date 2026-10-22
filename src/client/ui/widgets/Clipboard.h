#pragma once

#include <string>

namespace bs::client::ui::clipboard {

/// Text on the Windows clipboard as UTF-8 (empty if there is none or it cannot be opened).
[[nodiscard]] std::string getText();
/// Put UTF-8 text on the Windows clipboard. Returns false on failure.
bool setText(std::string const& utf8);

} // namespace bs::client::ui::clipboard
