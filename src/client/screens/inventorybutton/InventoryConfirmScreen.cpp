#ifdef BS_CLIENT

// Waystones' inventory-button confirm screen (vanilla ConfirmScreen): title at y=70, the line
// "Bound to: <name>" centred at y=100, Yes / No buttons (150x20) at max(height/6 + 96, ...)
// clamped to height - 24. Local screen: Yes sends `inventory_button.warp` to the server, which
// re-validates everything.

#include <algorithm>
#include <cmath>
#include <string>

#include "client/Client.h"
#include "client/net/ClientNet.h"
#include "client/ui/screen/Screens.h"
#include "client/ui/widgets/Style.h"
#include "client/ui/widgets/Widgets.h"
#include "core/inventorybutton/InventoryButton.h"
#include "mod/Log.h"

namespace bs::client::screens::inventorybutton {

using ui::Color;
using ui::FrameInfo;
using ui::MouseButton;
using ui::Painter;
using ui::TextAlign;

class InventoryConfirmScreen final : public ui::Screen {
public:
    InventoryConfirmScreen() {
        mYes.label   = ctr("bs.inventorybutton.yes", "Yes");
        mNo.label    = ctr("bs.inventorybutton.no", "No");
        mYes.onPress = [this] {
            log().info("inventory button: confirmed warp to {}", mName);
            net::sendMessage(
                core::inventorybutton::kWarpChannel,
                {
                    {"targetId", mTargetId}
            }
            );
            close();
        };
        mNo.onPress = [this] {
            log().info("inventory button: warp cancelled");
            close();
        };
    }

    void setModel(nlohmann::json const& model) override {
        mName     = model.value("name", std::string{});
        mTargetId = model.value("targetId", std::string{});
    }

    void render(Painter& p, FrameInfo const& f) override {
        drawBackground(p, f);
        float w = f.screen.w, h = f.screen.h;
        float x0 = f.screen.x, y0 = f.screen.y;
        p.text(
            ctr("gui.waystones.inventory.confirm_return", "Do you really want to warp back to this waystone?"),
            x0,
            y0 + 70,
            Color::rgb(ui::style::kWhite),
            true,
            1,
            TextAlign::Center,
            w
        );
        p.text(
            ctr("gui.waystones.inventory.confirm_return_bound_to", "Bound to: {}", mName),
            x0,
            y0 + 100,
            Color::rgb(ui::style::kGray),
            true,
            1,
            TextAlign::Center,
            w
        );
        float by  = std::clamp(std::floor(h / 6) + 96, 0.f, std::max(0.f, h - 24));
        mYes.rect = {x0 + std::floor(w / 2) - 155, y0 + by, 150, 20};
        mNo.rect  = {x0 + std::floor(w / 2) + 5, y0 + by, 150, 20};
        mYes.render(p, f.mouseX, f.mouseY);
        mNo.render(p, f.mouseX, f.mouseY);
    }

    bool onMouseButton(MouseButton b, bool down, float x, float y) override {
        if (!down) return false;
        if (mYes.click(b, x, y)) return true;
        return mNo.click(b, x, y);
    }

private:
    std::string mName;
    std::string mTargetId;
    ui::Button  mYes, mNo;
};

} // namespace bs::client::screens::inventorybutton

BS_REGISTER_CLIENT_SCREEN("inventory_confirm", [] {
    return std::make_unique<bs::client::screens::inventorybutton::InventoryConfirmScreen>();
});

#endif // BS_CLIENT
