#pragma once

// Drops repeated mouse-button presses (client build). The game can report one physical press
// twice (window messages and GameInput), which would cycle a button twice or pick an item up and
// put it straight back. A press counts once until its release, and a second press of the same
// button at the same spot within 50 ms is ignored.

#include <array>
#include <chrono>
#include <cmath>

#include "client/ui/UiLayer.h"

namespace bs::client::screens::edit {

class ClickFilter {
public:
    /// True when this press/release should be handled.
    bool accept(ui::MouseButton button, bool down, float x, float y) {
        auto& s   = mState[static_cast<std::size_t>(button)];
        auto  now = std::chrono::steady_clock::now();
        if (!down) {
            bool was = s.held;
            s.held   = false;
            return was;
        }
        bool repeat = s.held || (now - s.at < std::chrono::milliseconds(50) && std::abs(x - s.x) < 1 && std::abs(y - s.y) < 1);
        s.held      = true;
        s.at        = now;
        s.x         = x;
        s.y         = y;
        return !repeat;
    }

private:
    struct State {
        bool                                  held = false;
        std::chrono::steady_clock::time_point at{};
        float                                 x = -1, y = -1;
    };
    std::array<State, 3> mState{};
};

} // namespace bs::client::screens::edit
