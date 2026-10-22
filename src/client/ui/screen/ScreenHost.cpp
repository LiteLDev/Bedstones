#ifdef BS_CLIENT

// Hosts the native screens (see Screens.h). Adapted from AlleviatedBackpacks' native screen host:
// a dialog scene is pushed while a screen is open; its view's rendering is replaced by UiRoot,
// where `ScreenLayer` draws the screen.

#include "client/ui/screen/Screens.h"

#include <chrono>
#include <exception>
#include <memory>
#include <optional>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientExitLevelEvent.h"
#include "ll/api/event/render/UIRenderEvent.h"
#include "ll/api/memory/Hook.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/screens/AbstractScene.h"
#include "mc/client/gui/screens/SceneFactory.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/gui/screens/interfaces/ISceneStack.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"

#include "client/Client.h"
#include "client/net/ClientNet.h"
#include "client/ui/UiRoot.h"
#include "client/ui/widgets/Style.h"
#include "client/ui/widgets/TextInput.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace bs::client::ui {

struct ScreenHostAccess {
    static void init(Screen& s, std::string id, bool local) {
        s.mId    = std::move(id);
        s.mLocal = local;
    }
};

namespace {

using Clock               = std::chrono::steady_clock;
constexpr int VK_ESCAPE_ = 0x1B;

// ---- which screen is open --------------------------------------------------------------------------

struct Open {
    std::unique_ptr<Screen> screen;
    std::string             id;
    bool                    local   = false;
    std::uint32_t           session = 0;   ///< server screens
    std::uint64_t           rev     = 0;   ///< last applied server revision
    bool                    logged  = false;
};
Open                         gOpen;
std::optional<std::string>   gLocalId; ///< requested local screen
nlohmann::json               gLocalModel;
std::uint64_t                gLocalRev = 0, gLocalApplied = 0;

ScreenFactory const* factoryFor(std::string const& id) {
    for (auto const& e : ScreenFactoryRegistry::entries()) {
        if (e.id == id && e.create) return &e.create;
    }
    return nullptr;
}

void destroyOpen() {
    if (gOpen.screen) {
        try {
            gOpen.screen->onClosed();
        } catch (std::exception const& e) {
            log().error("screen {} onClosed failed: {}", gOpen.id, e.what());
        }
    }
    gOpen = {};
}

/// Brings `gOpen` in line with the wanted screen (local first, else the server's).
void syncScreen() {
    if (gLocalId) {
        if (!gOpen.screen || !gOpen.local || gOpen.id != *gLocalId || gLocalApplied != gLocalRev) {
            if (!gOpen.screen || !gOpen.local || gOpen.id != *gLocalId) {
                destroyOpen();
                auto const* f = factoryFor(*gLocalId);
                if (!f) {
                    log().warn("no native screen registered for local screen {}", *gLocalId);
                    gLocalId.reset();
                    return;
                }
                gOpen.screen = (*f)();
                gOpen.id     = *gLocalId;
                gOpen.local  = true;
                ScreenHostAccess::init(*gOpen.screen, gOpen.id, true);
            }
            gLocalApplied = gLocalRev;
            gOpen.screen->setModel(gLocalModel);
        }
        return;
    }
    auto const* srv = net::serverScreen();
    if (!srv) {
        if (gOpen.screen) destroyOpen();
        return;
    }
    if (!gOpen.screen || gOpen.local || gOpen.session != srv->session) {
        destroyOpen();
        auto const* f = factoryFor(srv->id);
        if (!f) {
            log().warn("no native screen registered for server screen {}; closing it", srv->id);
            net::closeServerScreen();
            return;
        }
        gOpen.screen  = (*f)();
        gOpen.id      = srv->id;
        gOpen.session = srv->session;
        ScreenHostAccess::init(*gOpen.screen, gOpen.id, false);
        gOpen.rev = 0;
    }
    if (gOpen.rev != net::screenRevision()) {
        gOpen.rev = net::screenRevision();
        gOpen.screen->setModel(srv->model);
    }
}

bool wantScreen() { return inLevel() && (gLocalId.has_value() || net::serverScreen() != nullptr); }

// ---- the layer ----------------------------------------------------------------------------------------

class ScreenLayer final : public IUiLayer {
public:
    bool rendering = false; ///< set by the view hook for our scene's frame

    [[nodiscard]] char const* name() const override { return "screen_host"; }
    [[nodiscard]] int         zOrder() const override { return 100; }
    [[nodiscard]] bool        visible(FrameInfo const&) const override { return rendering && gOpen.screen; }
    void                      render(Painter& p, FrameInfo const& frame) override {
        if (!gOpen.screen) return;
        gOpen.screen->render(p, frame);
        if (!gOpen.logged) {
            gOpen.logged = true;
            log().info("companion: screen {} shown", gOpen.id);
        }
    }
    bool onMouseButton(MouseButton b, bool down, float x, float y) override {
        if (gOpen.screen) gOpen.screen->onMouseButton(b, down, x, y);
        return true;
    }
    bool onMouseMove(float x, float y) override { return gOpen.screen && gOpen.screen->onMouseMove(x, y); }
    bool onScroll(float x, float y, float d) override {
        if (gOpen.screen) gOpen.screen->onScroll(x, y, d);
        return true;
    }
    bool onKey(int vk, bool down) override {
        if (!gOpen.screen) return false;
        if (gOpen.screen->onKey(vk, down)) return true;
        if (vk == VK_ESCAPE_ && down) gOpen.screen->close();
        return true;
    }
    bool onChar(std::string const& utf8) override { return gOpen.screen && gOpen.screen->onChar(utf8); }
    [[nodiscard]] bool wantsTextInput() const override { return gOpen.screen && gOpen.screen->wantsTextInput(); }
    [[nodiscard]] bool contains(float, float) const override { return true; }
    [[nodiscard]] bool capturesKey(int) const override { return true; }
};
ScreenLayer gLayer;
BS_REGISTER_UI_LAYER(gLayer);

// ---- the scene ----------------------------------------------------------------------------------------

struct SceneState {
    std::shared_ptr<AbstractScene> scene;
    std::shared_ptr<AbstractScene> retired;
    Clock::time_point              openedAt;
    bool                           seen         = false;
    bool                           closing      = false;
    bool                           popScheduled = false;
};
SceneState gScene;

ScreenView* ownView() {
    if (!gScene.scene) return nullptr;
    auto* scene = static_cast<UIScene*>(gScene.scene.get()); // the dialog factory creates UIScenes
    return scene->mScreenView.get();
}

bool owns(UIScene const* scene) { return gScene.scene && gScene.scene.get() == scene; }

ISceneStack* sceneStack() {
    auto* client = currentClient();
    if (!client) return nullptr;
    return client->getSceneFactory().getCurrentSceneStack().get();
}

/// The scene went away. `byPlayer`: Esc handled natively or the game dropped it: close the screen.
void dropScene(bool byPlayer) {
    if (byPlayer) {
        if (gLocalId) gLocalId.reset();
        else net::closeServerScreen();
        destroyOpen();
    }
    if (gScene.scene) gScene.retired = std::move(gScene.scene);
    gScene.scene.reset();
    gScene.seen         = false;
    gScene.closing      = false;
    gScene.popScheduled = false;
}

void pushScene() {
    auto* client = currentClient();
    auto  scene  = client ? client->getSceneFactory().createCommonDialogInfoScreen("Bedstones", "") : nullptr;
    if (!scene) {
        log().error("native screen: could not create the screen scene");
        return;
    }
    auto retired    = std::move(gScene.retired);
    gScene          = {};
    gScene.retired  = std::move(retired);
    gScene.scene    = std::move(scene);
    gScene.openedAt = Clock::now();
    client->getSceneFactory().getCurrentSceneStack()->pushScreen(gScene.scene, false);
}

/// Per-frame: push the scene for a new screen, pop it when the screen is gone.
void watch() {
    gScene.retired.reset();
    syncScreen();
    bool want = wantScreen() && gOpen.screen;
    if (want && !gScene.scene) pushScene();
    if (!gScene.scene) return;
    auto* stack = sceneStack();
    if (!stack) return;
    bool top = stack->getTopScene() == gScene.scene.get();
    if (top) gScene.seen = true;
    if (!want) gScene.closing = true;
    else if (gScene.closing && !gScene.popScheduled) gScene.closing = false; // reopened in time
    if (gScene.closing && top && !gScene.popScheduled) {
        stack->schedulePopScreen(1);
        gScene.popScheduled = true;
    }
    // Pushed but never made it to the top (dropped during a transition): give up.
    if (!top && !gScene.seen && Clock::now() - gScene.openedAt > std::chrono::seconds(3)) dropScene(true);
}

LL_TYPE_INSTANCE_HOOK(BsScreenViewHook, ll::memory::HookPriority::Highest, ScreenView, &ScreenView::render, void, ::UIRenderContext& ctx) {
    if (!gScene.scene || this != ownView()) return origin(ctx);
    try {
        watch();
        gLayer.rendering = !gScene.closing && gOpen.screen;
        UiRoot::get().renderFrame(*this, reinterpret_cast<MinecraftUIRenderContext&>(ctx));
    } catch (std::exception const& e) {
        log().error("native screen failed: {}", e.what());
    }
    gLayer.rendering = false;
}

LL_TYPE_INSTANCE_HOOK(
    BsSceneExitHook,
    ll::memory::HookPriority::Normal,
    UIScene,
    &UIScene::$onScreenExit,
    void,
    bool                           isPopping,
    bool                           doScreenTransitions,
    std::shared_ptr<AbstractScene> pushedScene
) {
    bool mine = owns(this);
    origin(isPopping, mine ? false : doScreenTransitions, std::move(pushedScene));
    if (mine && isPopping && owns(this)) dropScene(!gScene.closing);
}

LL_TYPE_INSTANCE_HOOK(BsSceneEntranceHook, ll::memory::HookPriority::Normal, UIScene, &UIScene::$onScreenEntrance, void, bool isRevisiting, bool doScreenTransitions) {
    origin(isRevisiting, owns(this) ? false : doScreenTransitions);
}

LL_TYPE_INSTANCE_HOOK(BsSceneBehindHook, ll::memory::HookPriority::Normal, UIScene, &UIScene::$renderGameBehind, bool) {
    if (owns(this)) return true;
    return origin();
}

LL_TYPE_INSTANCE_HOOK(BsSceneHurtHook, ll::memory::HookPriority::Normal, UIScene, &UIScene::$closeOnPlayerHurt, bool) {
    if (owns(this)) return false;
    return origin();
}

class ScreenHostModule {
public:
    void install() {
        TextInput::install();
        BsScreenViewHook::hook();
        BsSceneExitHook::hook();
        BsSceneEntranceHook::hook();
        BsSceneBehindHook::hook();
        BsSceneHurtHook::hook();
        auto& bus = ll::event::EventBus::getInstance();
        // Pushes the scene when a screen opens while another view is on top (e.g. the HUD).
        mRender = bus.emplaceListener<ll::event::AfterUIRenderEvent>([](auto&) {
            try {
                watch();
            } catch (std::exception const& e) {
                log().error("native screen host failed: {}", e.what());
            }
        });
        mExit = bus.emplaceListener<ll::event::ClientExitLevelEvent>([](auto&) {
            gLocalId.reset();
            destroyOpen();
            if (gScene.scene) dropScene(false); // the game flushes its scene stack itself
        });
    }
    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto* l : {&mRender, &mExit}) {
            if (*l) bus.removeListener(*l);
            l->reset();
        }
        if (gScene.scene) {
            if (auto* stack = sceneStack(); stack && stack->getTopScene() == gScene.scene.get()) stack->schedulePopScreen(1);
            dropScene(false);
        }
        gScene = {};
        gLocalId.reset();
        destroyOpen();
        BsSceneHurtHook::unhook();
        BsSceneBehindHook::unhook();
        BsSceneEntranceHook::unhook();
        BsSceneExitHook::unhook();
        BsScreenViewHook::unhook();
        TextInput::uninstall();
    }

private:
    ll::event::ListenerPtr mRender;
    ll::event::ListenerPtr mExit;
};

} // namespace

// ---- Screen -----------------------------------------------------------------------------------------

void Screen::action(std::string const& name, nlohmann::json args) {
    if (mLocal) {
        onLocalAction(name, args);
        return;
    }
    net::sendScreenAction(name, std::move(args));
}

void Screen::close() {
    if (gOpen.screen.get() != this) return;
    if (mLocal) gLocalId.reset();
    else net::closeServerScreen();
    // The host pops the scene next frame (no screen wanted any more) and destroys this screen.
}

void Screen::drawBackground(Painter& painter, FrameInfo const& frame) { style::screenBackground(painter, frame.screen); }

namespace screens {
void openLocal(std::string const& id, nlohmann::json model) {
    gLocalId    = id;
    gLocalModel = std::move(model);
    ++gLocalRev;
}

void closeCurrent() {
    if (gOpen.screen) gOpen.screen->close();
    gLocalId.reset();
}

Screen* current() { return gOpen.screen.get(); }

bool active() { return gOpen.screen && gScene.scene && !gScene.closing; }
} // namespace screens

} // namespace bs::client::ui

BS_REGISTER_MODULE_ORDERED(bs::client::ui::ScreenHostModule, bs::ModuleOrder::Feature + 102);

#endif // BS_CLIENT
