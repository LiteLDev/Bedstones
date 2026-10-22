#pragma once

// Input routing for the client companion (ported from BEI's InputRouter).
//
// - Mouse/keyboard events arrive on the window thread. The router decides synchronously whether
//   to swallow an event (from the last rendered frame's hit tests) and runs the real handling on
//   the client thread (once per frame, from ClientInstance::update).
// - Mouse buttons and the wheel go to the UI layers (UiRoot::dispatch*); they are swallowed only
//   when the cursor is over a layer (a release is swallowed iff its press was). Mouse moves are
//   forwarded too (drags, hover).
// - Keys: while a layer has text focus every key goes to it; a layer may also capture specific
//   keys (`IUiLayer::capturesKey`, e.g. the native screen's B / [ / ] / Q / 1-9); everything else
//   goes to vanilla.

#include <functional>
#include <optional>
#include <string>


namespace bs::client::input {

/// Queue `fn` to run on the client thread at the next ClientInstance::update. Any thread.
void post(std::function<void()> fn);

/// Physical modifier state (window thread view, readable anywhere).
[[nodiscard]] bool shiftDown();
[[nodiscard]] bool ctrlDown();
[[nodiscard]] bool altDown();

} // namespace bs::client::input
