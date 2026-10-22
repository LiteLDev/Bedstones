#ifdef BS_CLIENT

// Inventory button, client side: the 16x16 "Return to Waystone" button drawn over the vanilla
// inventory (docs/SPEC.md §3.8). The server sends the state (mode, target, affordability and the
// configured positions) on `inventory_button.state`; a click opens the local confirm screen
// (NEAREST / named) or asks the server directly (ANY).
//
// Placement: Java puts the button at (inventoryButtonX, inventoryButtonY) relative to the
// inventory image's top-left (AbstractContainerScreen: ((w - 176) / 2, (h - 166) / 2), moved
// right by the open recipe book). Bedrock's classic inventory has the same 176x166 panel
// ("player_inventory") next to its recipe book; PanelLocator reads its live position, and the
// offsets apply to it unchanged. Bedrock's creative inventory shows that panel only on its
// survival-inventory (crafting) tab, and the button appears only there, like in Java.

#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <vector>

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/item/ItemStack.h"

#include "client/Client.h"
#include "client/inventorybutton/PanelLocator.h"
#include "client/net/ClientNet.h"
#include "client/ui/UiRoot.h"
#include "client/ui/screen/Screens.h"
#include "client/ui/widgets/Style.h"
#include "client/ui/widgets/Tooltip.h"
#include "core/inventorybutton/InventoryButton.h"
#include "game/Ids.h"
#include "game/Items.h"
#include "mod/Log.h"

namespace bs::client::inventorybutton {

using core::inventorybutton::ButtonState;
using core::inventorybutton::Mode;
using ui::Color;
using ui::FrameInfo;
using ui::MouseButton;
using ui::Painter;
using ui::PixelArt;
using ui::Rect;

namespace {

ButtonState gState;

// ---- procedural icons (original pixel art) -------------------------------------------------------------

// A parchment scroll with a seal; the seal color tells bound (aqua) from warp (purple).
constexpr std::string_view kScrollRows[] = {
    "................",
    "................",
    "..DDDDDDDDDDDD..",
    ".DSPPPPPPPPPPSD.",
    ".DSPPPPPPPPPPSD.",
    "..DPPPPPPPPPPD..",
    "..DPLLLLLLLLPD..",
    "..DPPPPPPPPPPD..",
    "..DPLLLLLLPPPD..",
    "..DPPPPRRPPPPD..",
    "..DPPPRrrRPPPD..",
    "..DPPPPRRPPPPD..",
    ".DSPPPPPPPPPPSD.",
    ".DSPPPPPPPPPPSD.",
    "..DDDDDDDDDDDD..",
    "................",
};
// The warp scroll glows: sparkles around the parchment.
constexpr std::string_view kWarpRows[] = {
    "..G..........G..",
    "......G.........",
    "..DDDDDDDDDDDD..",
    ".DSPPPPPPPPPPSD.",
    ".DSPPPPPPPPPPSDG",
    "..DPPPPPPPPPPD..",
    "G.DPLLLLLLLLPD..",
    "..DPPPPPPPPPPD..",
    "..DPLLLLLLPPPD..",
    "..DPPPPRRPPPPD..",
    "..DPPPRrrRPPPD.G",
    "..DPPPPRRPPPPD..",
    ".DSPPPPPPPPPPSD.",
    ".DSPPPPPPPPPPSD.",
    "G.DDDDDDDDDDDD..",
    "..........G.....",
};
// The `inventory/inventory_button` sprite: a small waystone with a rune.
constexpr std::string_view kButtonRows[] = {
    "................",
    ".....DDDDDD.....",
    "....DLLLLLLD....",
    "....DLGGGGLD....",
    ".....DGRRGD.....",
    ".....DGRRGD.....",
    ".....DGGGGD.....",
    ".....DGRRGD.....",
    ".....DGGGGD.....",
    ".....DGGGGD.....",
    "....DLGGGGLD....",
    "....DLLLLLLD....",
    "...DLLLLLLLLD...",
    "...DGGGGGGGGD...",
    "...DDDDDDDDDD...",
    "................",
};

PixelArt art(std::span<std::string_view const> rows, std::vector<std::string_view> palette) {
    return PixelArt{
        {rows.begin(), rows.end()},
        std::move(palette)
    };
}

PixelArt const& boundArt() {
    static PixelArt a = art(kScrollRows, {"D=5A4020", "S=B89660", "P=EED9A6", "L=C4A672", "R=1E8C8C", "r=55FFFF"});
    return a;
}
PixelArt const& warpArt() {
    static PixelArt a =
        art(kWarpRows, {"D=5A4020", "S=B89660", "P=EED9A6", "L=C4A672", "R=7A2FB0", "r=E070FF", "G=D88CFF"});
    return a;
}
PixelArt const& buttonArt() {
    static PixelArt a = art(kButtonRows, {"D=2E2E36", "L=A8A8B0", "G=7C7C86", "R=B060FF"});
    return a;
}

// ---- item icons (when the scroll items exist) -----------------------------------------------------------

struct Icons {
    bool                       tried = false;
    std::unique_ptr<ItemStack> bound, warp;
};
Icons gIcons;

void loadIcons() {
    if (gIcons.tried) return;
    gIcons.tried = true;
    auto make    = [](std::string_view path) -> std::unique_ptr<ItemStack> {
        try {
            auto s = std::make_unique<ItemStack>(game::makeItem(ids::id(path)));
            if (s->isNull()) return nullptr;
            return s;
        } catch (...) {
            return nullptr;
        }
    };
    gIcons.bound = make(ids::kBoundScroll);
    gIcons.warp  = make(ids::kWarpScroll);
}

// ---- screens ------------------------------------------------------------------------------------------

/// The survival inventory panel when the frame shows one (survival inventory, or the creative
/// inventory's survival tab).
std::optional<Rect> panelOf(FrameInfo const& f) {
    if (!f.inLevel || (f.screenName != "inventory_screen" && f.screenName != "inventory_screen_pocket"))
        return std::nullopt;
    return inventoryPanel();
}

// ---- the layer ----------------------------------------------------------------------------------------

class InventoryButtonLayer final : public ui::IUiLayer {
public:
    [[nodiscard]] char const* name() const override { return "inventory_button"; }
    [[nodiscard]] int         zOrder() const override { return 10; }

    [[nodiscard]] bool visible(FrameInfo const& f) const override {
        bool show = gState.mode != Mode::None && !ui::screens::active() && panelOf(f).has_value();
        if (!show) {
            setRect({});
            mLogged = false;
        }
        return show;
    }

    void render(Painter& p, FrameInfo const& f) override {
        auto panel = panelOf(f);
        if (!panel) return;
        Rect r{panel->x + static_cast<float>(gState.x), panel->y + static_cast<float>(gState.y), 16, 16};
        if (r.x != mRect.x || r.y != mRect.y) mMovedAt = f.time; // the screen slides in when opened
        setRect(r);
        if (!mLogged && f.time - mMovedAt > 0.3) {
            mLogged = true;
            log().info(
                "inventory button: shown at {},{} on {} ({}x{})",
                mRect.x,
                mRect.y,
                f.screenName,
                f.screen.w,
                f.screen.h
            );
        }

        bool hovered = mRect.contains(f.mouseX, f.mouseY);
        if (!gState.canAfford) {
            p.pixels(buttonArt(), mRect.x, mRect.y, 1, Painter::Layer::Foreground, 0.5f);
        } else {
            loadIcons();
            auto const& stack = hovered ? gIcons.warp : gIcons.bound;
            if (stack) p.item(*stack, mRect.x, mRect.y);
            else p.pixels(hovered ? warpArt() : boundArt(), mRect.x, mRect.y, 1, Painter::Layer::Foreground);
        }
        if (hovered) {
            if (!mTooltipLogged) {
                mTooltipLogged = true;
                log().info("inventory button: tooltip shown");
            }
            p.flush();
            ui::drawTooltip(p, tooltip(), f.mouseX, f.mouseY);
        } else {
            mTooltipLogged = false;
        }
    }

    bool onMouseButton(MouseButton button, bool down, float x, float y) override {
        if (!mRect.contains(x, y)) return false;
        if (down && button == MouseButton::Left) press();
        return true; // never let vanilla see a click on the button
    }
    /// Window thread (InputRouter decides there whether a click is ours).
    [[nodiscard]] bool contains(float x, float y) const override {
        std::lock_guard lock(mHitMutex);
        return mHit.contains(x, y);
    }

    void reset() {
        mLogged = false;
        setRect({});
    }

private:
    static std::vector<ui::TooltipLine> tooltip() {
        std::vector<ui::TooltipLine> lines;
        bool                         nearest = gState.mode == Mode::Nearest;
        lines.push_back(
            {nearest ? ctr("gui.waystones.inventory.return_to_nearest_waystone", "Return to Nearest Waystone")
                     : ctr("gui.waystones.inventory.return_to_waystone", "Return to Waystone"),
             ui::style::kYellow}
        );
        if (!gState.hasTarget) {
            lines.push_back(
                {ctr("gui.waystones.inventory.no_waystones_activated", "You have not activated any waystones yet."),
                 ui::style::kRed}
            );
        } else if (gState.mode != Mode::Any) {
            lines.push_back(
                {ctr("gui.waystones.inventory.confirm_return_bound_to", "Bound to: {}", "§3" + gState.targetName),
                 ui::style::kGray}
            );
        }
        if (!gState.canAfford) {
            if (gState.cooldownSeconds > 0) {
                lines.push_back(
                    {ctr("bs.inventorybutton.cooldown", "Cooldown: {} seconds", gState.cooldownSeconds),
                     ui::style::kGold}
                );
            }
            if (gState.requirement.xpPoints > 0) {
                lines.push_back(
                    {ctr("bs.inventorybutton.xp_cost", "Experience: {} points", gState.requirement.xpPoints),
                     ui::style::kRed}
                );
            }
        }
        return lines;
    }

    static void click() {
        if (auto* c = currentClient()) c->playUI("random.click", 1.f, 1.f);
    }

    static void press() {
        click();
        if (!gState.canAfford) {
            log().info("inventory button: clicked while unaffordable");
            return;
        }
        if (gState.mode == Mode::Any) {
            log().info("inventory button: clicked (any)");
            net::sendMessage(core::inventorybutton::kWarpChannel, nlohmann::json::object());
            return;
        }
        if (!gState.hasTarget) {
            log().info("inventory button: clicked without a target");
            return;
        }
        log().info("inventory button: clicked, confirming {}", gState.targetName);
        ui::screens::openLocal(
            "inventory_confirm",
            {
                {"name",     gState.targetName},
                {"targetId", gState.targetId  }
        }
        );
    }

    void setRect(Rect const& r) const {
        mRect = r;
        std::lock_guard lock(mHitMutex);
        mHit = r;
    }

    mutable Rect       mRect; ///< client thread
    mutable Rect       mHit;  ///< copy for the window thread
    mutable std::mutex mHitMutex;
    double             mMovedAt       = 0;
    mutable bool       mLogged        = false;
    bool               mTooltipLogged = false;
};

InventoryButtonLayer gLayer;
BS_REGISTER_UI_LAYER(gLayer);

} // namespace

} // namespace bs::client::inventorybutton

BS_REGISTER_SERVER_MESSAGE(bs::core::inventorybutton::kStateChannel, [](nlohmann::json const& data) {
    auto& s = bs::client::inventorybutton::gState;
    auto  n = bs::core::inventorybutton::ButtonState::fromJson(data);
    if (n.mode != s.mode) bs::log().info("inventory button: mode {}", bs::core::inventorybutton::toString(n.mode));
    s = std::move(n);
});

namespace {
struct LeaveWorldRegistrar {
    LeaveWorldRegistrar() {
        bs::client::net::onLeaveWorld([] {
            bs::client::inventorybutton::gState = {};
            bs::client::inventorybutton::gIcons = {};
            bs::client::inventorybutton::gLayer.reset();
        });
    }
} gLeaveWorldRegistrar;
} // namespace

#endif // BS_CLIENT
