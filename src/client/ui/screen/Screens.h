#pragma once

// Native screens of the client mod (client build only, client thread).
//
// A `Screen` draws a whole Waystones screen (selection, edit, manage, ...) on top of the dimmed
// world and handles its input. Screens are registered by id:
//
//     class SelectionScreen : public bs::client::ui::Screen { ... };
//     BS_REGISTER_CLIENT_SCREEN("selection", [] { return std::make_unique<SelectionScreen>(); });
//
// Server screens: the server opens them (net/Screens.h); the host creates the registered screen,
// passes the model to `setModel` (again on every update) and routes `action()` to the server.
// Local screens (e.g. a confirm dialog that needs no server state) are opened with
// `screens::openLocal(id, model)`; their actions go to `onLocalAction`.
//
// Hosting: the host pushes a dialog scene (gives a free cursor, keeps the world rendered behind,
// pauses nothing) and replaces that scene's view rendering with the screen. Esc closes the screen
// unless the screen consumes it (e.g. to save first, like Waystones' edit screen).
// The host logs "companion: screen <id> shown" the first frame a screen is drawn (e2e hook).

#include <functional>
#include <memory>
#include <string>

#include "nlohmann/json.hpp"

#include "client/ui/UiLayer.h"
#include "client/ui/gfx/Painter.h"
#include "mod/Registry.h"

namespace bs::client::ui {

class Screen {
public:
    virtual ~Screen() = default;

    /// The model arrived or changed (server screens: on open and every update).
    virtual void setModel(nlohmann::json const& /*model*/) {}
    /// Draws the screen; `frame.screen` is the whole GUI area. Call `drawBackground` first for
    /// vanilla's dimmed background.
    virtual void render(Painter& painter, FrameInfo const& frame) = 0;

    virtual bool onMouseButton(MouseButton /*button*/, bool /*down*/, float /*x*/, float /*y*/) { return false; }
    virtual bool onMouseMove(float /*x*/, float /*y*/) { return false; }
    virtual bool onScroll(float /*x*/, float /*y*/, float /*delta*/) { return false; }
    /// Windows virtual-key code. Return true to consume (Esc not consumed closes the screen).
    virtual bool onKey(int /*vk*/, bool /*down*/) { return false; }
    virtual bool onChar(std::string const& /*utf8*/) { return false; }
    [[nodiscard]] virtual bool wantsTextInput() const { return false; }
    /// The screen is going away (closed by the player, the server or replaced).
    virtual void onClosed() {}
    /// Actions of local screens (`openLocal`).
    virtual void onLocalAction(std::string const& /*action*/, nlohmann::json const& /*args*/) {}

    /// Sends an action: to the server for server screens, to `onLocalAction` for local ones.
    void action(std::string const& name, nlohmann::json args = nlohmann::json::object());
    /// Closes this screen (server screens tell the server).
    void close();

    [[nodiscard]] bool               isLocal() const { return mLocal; }
    [[nodiscard]] std::string const& id() const { return mId; }

    /// Vanilla dimmed background over the whole frame.
    static void drawBackground(Painter& painter, FrameInfo const& frame);

private:
    friend struct ScreenHostAccess;
    bool        mLocal = false;
    std::string mId;
};

using ScreenFactory = std::function<std::unique_ptr<Screen>()>;

struct ScreenFactoryEntry {
    std::string   id;
    ScreenFactory create;
};
using ScreenFactoryRegistry = StaticRegistry<ScreenFactoryEntry>;

namespace screens {
/// Opens a local (client-only) screen, replacing any open screen.
void openLocal(std::string const& id, nlohmann::json model = nlohmann::json::object());
/// Closes whatever screen is open.
void closeCurrent();
/// The open screen (nullptr when none).
[[nodiscard]] Screen* current();
/// True while a native screen is drawn this frame.
[[nodiscard]] bool active();
} // namespace screens

namespace detail {
struct ScreenFactoryRegistrar {
    ScreenFactoryRegistrar(std::string id, ScreenFactory f) { ScreenFactoryRegistry::add({std::move(id), std::move(f)}); }
};
} // namespace detail

} // namespace bs::client::ui

/// Registers the native screen for id: BS_REGISTER_CLIENT_SCREEN("edit", [] { return std::make_unique<EditScreen>(); });
#define BS_REGISTER_CLIENT_SCREEN(id, ...)                                                                             \
    static ::bs::client::ui::detail::ScreenFactoryRegistrar BS_UNIQUE(bs_client_screen_)(id, __VA_ARGS__)
