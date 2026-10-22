#ifdef BS_CLIENT

// Finds the vanilla inventory panel in the inventory screen's visual tree.
//
// Bedrock's classic inventory ("inventory_screen") lays out a horizontal stack: the recipe book
// (146 wide, or the 326-wide creative item catalog) and the "player_inventory" panel, 176x166
// exactly like Java's InventoryScreen image. Java centres that image (AbstractContainerScreen:
// ((w - 176) / 2, (h - 166) / 2), shifted right by the open recipe book); Bedrock centres the
// whole stack, and the screen slides in when opened. So instead of recomputing the layout we read
// the panel's live position from the screen's controls every frame.

#include "client/inventorybutton/PanelLocator.h"

#include <chrono>
#include <exception>
#include <memory>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/render/UIRenderEvent.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/gui/controls/UIControl.h"
#include "mc/client/gui/controls/VisualTree.h"
#include "mc/client/gui/screens/ScreenView.h"
#include "mc/client/gui/screens/UIScene.h"
#include "mc/client/renderer/screen/MinecraftUIRenderContext.h"

#include "client/Client.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace bs::client::inventorybutton {

namespace {

using Clock = std::chrono::steady_clock;

constexpr char const* kPanelControl = "player_inventory";

std::optional<ui::Rect> gPanel;
Clock::time_point       gSeen;
bool                    gErrorLogged = false;

bool isInventoryScreen(std::string const& name) {
    return name == "inventory_screen" || name == "inventory_screen_pocket";
}

void locate(ScreenView& view, MinecraftUIRenderContext& ctx) {
    auto* client = currentClient();
    if (!client || !view.mHasFocus) return;
    auto name = client->getScreenName();
    if (!isInventoryScreen(name) || ctx.mCurrentScene.getScreenName() != name) return;
    VisualTree* tree = view.mVisualTree.get();
    if (!tree) return;
    auto control = tree->getControlByName(kPanelControl, true);
    if (!control || !control->mIsVisibleInTree) {
        gPanel.reset();
        return;
    }
    auto pos  = *control->mCachedPosition;
    auto size = *control->mSize;
    if (size.x <= 0 || size.y <= 0) return;
    gPanel = ui::Rect{pos.x, pos.y, size.x, size.y};
    gSeen  = Clock::now();
}

class PanelLocatorModule {
public:
    void install() {
        mListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::AfterUIRenderEvent>([](auto& ev) {
            try {
                locate(ev.screenView(), ev.uiRenderContext());
            } catch (std::exception const& e) {
                if (!gErrorLogged) log().error("inventory button: panel locator failed: {}", e.what());
                gErrorLogged = true;
            }
        });
    }
    void uninstall() {
        if (mListener) ll::event::EventBus::getInstance().removeListener(mListener);
        mListener.reset();
        gPanel.reset();
    }

private:
    ll::event::ListenerPtr mListener;
};

} // namespace

std::optional<ui::Rect> inventoryPanel() {
    // Seen in the last frames (the layer may draw before or after the locator in a frame).
    if (!gPanel || Clock::now() - gSeen > std::chrono::milliseconds(250)) return std::nullopt;
    return gPanel;
}

} // namespace bs::client::inventorybutton

BS_REGISTER_MODULE(bs::client::inventorybutton::PanelLocatorModule);

#endif // BS_CLIENT
