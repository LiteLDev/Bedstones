#pragma once

#ifdef BS_CLIENT

#include <array>

#include "client/ui/UiLayer.h"

namespace bs::client::screens::groups {

/// Drops repeated mouse-button events: the input pipeline can deliver one physical press twice
/// (seen with headless input), which would cycle an icon two steps or send an action twice.
/// A press counts once until its release.
class PressFilter {
public:
    /// True when the event should be handled.
    bool accept(ui::MouseButton button, bool down) {
        auto& held = mHeld[static_cast<std::size_t>(button)];
        if (down == held) return false;
        held = down;
        return true;
    }

private:
    std::array<bool, 3> mHeld{};
};

} // namespace bs::client::screens::groups

#endif
