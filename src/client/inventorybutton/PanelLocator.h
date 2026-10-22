#pragma once

#ifdef BS_CLIENT

#include <optional>

#include "client/ui/Geometry.h"

namespace bs::client::inventorybutton {

/// The vanilla survival inventory panel ("player_inventory" control, 176x166 like Java's
/// InventoryScreen) of the inventory screen drawn in the last frames, in GUI units. nullopt when
/// no inventory screen shows it (other screens, the creative item catalog). Client thread.
[[nodiscard]] std::optional<ui::Rect> inventoryPanel();

} // namespace bs::client::inventorybutton

#endif // BS_CLIENT
