#ifdef BS_CLIENT

// Native Warp Modifiers screen (docs/SPEC.md §4.6): a 176x196 container panel drawn
// procedurally (original look: vanilla-style panel with the five slots in a cross joined by
// grooves), title "Warp Modifiers", the galactic name top right, the player inventory at y 104
// and the hotbar at y 162. Every click goes to the server, which owns the items (the cursor item
// too) and sends the model back.

#include <array>
#include <cmath>
#include <optional>
#include <format>
#include <string>
#include <vector>

#include "mc/world/item/ItemStack.h"

#include "client/Client.h"
#include "client/screens/edit/Icons.h"
#include "client/ui/screen/Screens.h"
#include "client/ui/widgets/Style.h"
#include "client/ui/widgets/Tooltip.h"
#include "client/ui/widgets/Widgets.h"
#include "core/modifiers/Modifiers.h"
#include "game/Items.h"
#include "mod/Log.h"

namespace bs::client::screens::edit {

namespace {

using namespace bs::client::ui;
namespace cm = core::modifiers;

constexpr float kWidth  = 176;
constexpr float kHeight = 196;
constexpr int   kVkShift = 0x10, kVkLShift = 0xA0, kVkRShift = 0xA1, kVkE = 0x45;

/// Slot positions (top-left of the 16x16 item area) in menu slot order.
struct SlotPos {
    float x, y;
};
constexpr SlotPos kContainerPos[cm::kContainerSlots] = {{80, 45}, {80, 17}, {108, 45}, {80, 73}, {52, 45}};

SlotPos inventoryPos(int i) {
    if (i < 9) return {8.f + 18.f * static_cast<float>(i), 162};
    int r = (i - 9) / 9, c = (i - 9) % 9;
    return {8.f + 18.f * static_cast<float>(c), 104.f + 18.f * static_cast<float>(r)};
}

struct ShownItem {
    int       count = 0;
    ItemStack stack;
};

std::optional<ShownItem> shown(nlohmann::json const& j) {
    if (!j.is_object()) return std::nullopt;
    auto id = j.value("id", std::string{});
    int  n  = j.value("count", 0);
    if (id.empty() || n <= 0) return std::nullopt;
    ShownItem s;
    s.count = n;
    s.stack = game::makeItem(id, n, j.value("aux", 0));
    return s;
}

// ---- procedural panel ------------------------------------------------------------------------------

void panel(Painter& p, Rect const& r) {
    auto c = [](std::uint32_t argb) { return Color::argb(argb); };
    // Rounded black outline, white top-left bevel, gray bottom-right bevel, light gray face.
    p.fillRect({r.x + 2, r.y, r.w - 4, 1}, c(0xFF000000));
    p.fillRect({r.x + 2, r.bottom() - 1, r.w - 4, 1}, c(0xFF000000));
    p.fillRect({r.x, r.y + 2, 1, r.h - 4}, c(0xFF000000));
    p.fillRect({r.right() - 1, r.y + 2, 1, r.h - 4}, c(0xFF000000));
    p.fillRect({r.x + 1, r.y + 1, 1, 1}, c(0xFF000000));
    p.fillRect({r.right() - 2, r.y + 1, 1, 1}, c(0xFF000000));
    p.fillRect({r.x + 1, r.bottom() - 2, 1, 1}, c(0xFF000000));
    p.fillRect({r.right() - 2, r.bottom() - 2, 1, 1}, c(0xFF000000));
    p.fillRect({r.x + 1, r.y + 2, r.w - 2, r.h - 4}, c(0xFFC6C6C6));
    p.fillRect({r.x + 2, r.y + 1, r.w - 4, r.h - 2}, c(0xFFC6C6C6));
    p.fillRect({r.x + 2, r.y + 1, r.w - 5, 2}, c(0xFFFFFFFF));
    p.fillRect({r.x + 1, r.y + 2, 2, r.h - 5}, c(0xFFFFFFFF));
    p.fillRect({r.x + 3, r.bottom() - 3, r.w - 5, 2}, c(0xFF555555));
    p.fillRect({r.right() - 3, r.y + 3, 2, r.h - 5}, c(0xFF555555));
}

void slotFrame(Painter& p, float x, float y) {
    p.fillRect({x - 1, y - 1, 18, 18}, Color::argb(0xFF8B8B8B));
    p.fillRect({x - 1, y - 1, 17, 1}, Color::argb(0xFF373737));
    p.fillRect({x - 1, y - 1, 1, 17}, Color::argb(0xFF373737));
    p.fillRect({x, y + 16, 17, 1}, Color::argb(0xFFFFFFFF));
    p.fillRect({x + 16, y, 1, 17}, Color::argb(0xFFFFFFFF));
}

/// The cross: grooves joining the outer slots to the shard slot, and a violet rim on slot 0.
void cross(Painter& p, float L, float T) {
    auto groove = Color::argb(0xFF9A9A9A);
    auto light  = Color::argb(0xFFE2E2E2);
    p.fillRect({L + 86, T + 34, 4, 10}, groove);   // up
    p.fillRect({L + 86, T + 62, 4, 10}, groove);   // down
    p.fillRect({L + 69, T + 51, 10, 4}, groove);   // left
    p.fillRect({L + 97, T + 51, 10, 4}, groove);   // right
    p.fillRect({L + 90, T + 34, 1, 10}, light);
    p.fillRect({L + 90, T + 62, 1, 10}, light);
    p.fillRect({L + 69, T + 55, 10, 1}, light);
    p.fillRect({L + 97, T + 55, 10, 1}, light);
    auto rim = Color::argb(0xFF7A4BA8);
    p.fillRect({L + 77, T + 42, 22, 1}, rim);
    p.fillRect({L + 77, T + 63, 22, 1}, rim);
    p.fillRect({L + 77, T + 42, 1, 22}, rim);
    p.fillRect({L + 98, T + 42, 1, 22}, rim);
}

void drawCount(Painter& p, int count, float x, float y) {
    if (count <= 1) return;
    auto s = std::to_string(count);
    p.text(s, x + 17 - p.textWidth(s), y + 9, Color::rgb(style::kWhite), true);
}

class ModifiersScreen final : public Screen {
public:
    void setModel(nlohmann::json const& m) override {
        mGalactic      = m.value("galactic", std::string{});
        mGalacticColor = style::formattingColor(m.value("galacticColor", std::string{"white"}));
        auto container = m.value("container", nlohmann::json::array());
        auto inventory = m.value("inventory", nlohmann::json::array());
        for (int i = 0; i < cm::kContainerSlots; ++i) {
            mContainer[static_cast<std::size_t>(i)] = i < static_cast<int>(container.size()) ? shown(container[static_cast<std::size_t>(i)]) : std::nullopt;
        }
        for (int i = 0; i < cm::kInventorySlots; ++i) {
            mInventory[static_cast<std::size_t>(i)] = i < static_cast<int>(inventory.size()) ? shown(inventory[static_cast<std::size_t>(i)]) : std::nullopt;
        }
        mCursor = shown(m.value("cursor", nlohmann::json{}));
    }

    void render(Painter& p, FrameInfo const& f) override {
        drawBackground(p, f);
        mLeft = std::floor((f.screen.w - kWidth) / 2);
        mTop  = std::floor((f.screen.h - kHeight) / 2);
        if (!mLoggedLayout) {
            mLoggedLayout = true;
            log().info("modifiers screen: layout gui={}x{} left={} top={}", f.screen.w, f.screen.h, mLeft, mTop);
        }
        float L = mLeft, T = mTop, mx = f.mouseX, my = f.mouseY;
        panel(p, {L, T, kWidth, kHeight});
        cross(p, L, T);
        for (auto const& s : kContainerPos) slotFrame(p, L + s.x, T + s.y);
        for (int i = 0; i < cm::kInventorySlots; ++i) {
            auto s = inventoryPos(i);
            slotFrame(p, L + s.x, T + s.y);
        }
        p.flush();

        auto label = Color::rgb(style::kLabel);
        p.text(ctr("container.waystones.warp_modifiers", "Warp Modifiers"), L + 8, T + 6, label);
        p.text(ctr("bs.modifiers.inventory", "Inventory"), L + 8, T + 93, label);
        drawGalactic(p, L + kWidth - 8, T + 6);

        int hovered = slotAt(mx, my);
        auto drawSlot = [&](std::optional<ShownItem> const& item, float x, float y, int id) {
            if (item) {
                p.item(item->stack, x, y);
                drawCount(p, item->count, x, y);
            }
            if (hovered == id) p.fillRect({x, y, 16, 16}, Color::argb(0x80FFFFFF), Painter::Layer::Foreground);
        };
        for (int i = 0; i < cm::kContainerSlots; ++i) {
            drawSlot(mContainer[static_cast<std::size_t>(i)], L + kContainerPos[i].x, T + kContainerPos[i].y, i);
        }
        for (int i = 0; i < cm::kInventorySlots; ++i) {
            auto s = inventoryPos(i);
            drawSlot(mInventory[static_cast<std::size_t>(i)], L + s.x, T + s.y, cm::kInventoryBase + i);
        }
        p.flush();

        if (mCursor) {
            p.item(mCursor->stack, mx - 8, my - 8);
            drawCount(p, mCursor->count, mx - 8, my - 8);
            p.flush();
        } else if (hovered >= 0) {
            auto const* item = itemAt(hovered);
            if (item && *item) drawTooltip(p, itemTooltipLines((*item)->stack), mx, my);
        }
    }

    bool onMouseButton(MouseButton b, bool down, float x, float y) override {
        if (!down || b == MouseButton::Middle) return true;
        int  button = b == MouseButton::Right ? 1 : 0;
        int  slot   = slotAt(x, y);
        bool inside = Rect{mLeft, mTop, kWidth, kHeight}.contains(x, y);
        if (slot < 0) {
            if (!inside && mCursor) action("click", {{"slot", cm::kOutside}, {"button", button}});
            return true;
        }
        if (mShift && button == 0) action("quick_move", {{"slot", slot}});
        else action("click", {{"slot", slot}, {"button", button}});
        return true;
    }

    bool onKey(int vk, bool down) override {
        if (vk == kVkShift || vk == kVkLShift || vk == kVkRShift) {
            mShift = down;
            return true;
        }
        if (vk == kVkE && down) {
            close(); // the inventory key closes container screens
            return true;
        }
        return false;
    }

private:
    [[nodiscard]] int slotAt(float x, float y) const {
        auto hit = [&](SlotPos s) { return Rect{mLeft + s.x - 1, mTop + s.y - 1, 18, 18}.contains(x, y); };
        for (int i = 0; i < cm::kContainerSlots; ++i) {
            if (hit(kContainerPos[i])) return i;
        }
        for (int i = 0; i < cm::kInventorySlots; ++i) {
            if (hit(inventoryPos(i))) return cm::kInventoryBase + i;
        }
        return -1;
    }

    [[nodiscard]] std::optional<ShownItem> const* itemAt(int id) const {
        if (id >= 0 && id < cm::kContainerSlots) return &mContainer[static_cast<std::size_t>(id)];
        int inv = id - cm::kInventoryBase;
        if (inv >= 0 && inv < cm::kInventorySlots) return &mInventory[static_cast<std::size_t>(inv)];
        return nullptr;
    }

    /// The galactic identifier right-aligned at (right, y), 3x5 glyphs scaled to the font height.
    void drawGalactic(Painter& p, float right, float y) const {
        if (mGalactic.empty()) return;
        constexpr float kScale = 1.5f, kAdvance = 4 * kScale;
        float           x      = right - kAdvance * static_cast<float>(mGalactic.size()) + kScale;
        auto            hex    = std::format("X={:06X}", mGalacticColor);
        for (char c : mGalactic) {
            PixelArt glyph = icons::galacticGlyph(c);
            glyph.palette  = {hex};
            p.pixels(glyph, x, y, kScale, Painter::Layer::Foreground);
            x += kAdvance;
        }
    }

    std::array<std::optional<ShownItem>, cm::kContainerSlots> mContainer{};
    std::array<std::optional<ShownItem>, cm::kInventorySlots> mInventory{};
    std::optional<ShownItem>                                  mCursor;
    std::string                                               mGalactic;
    std::uint32_t                                             mGalacticColor = style::kWhite;
    float                                                     mLeft = 0, mTop = 0;
    bool                                                      mShift        = false;
    bool                                                      mLoggedLayout = false;
};

} // namespace

} // namespace bs::client::screens::edit

BS_REGISTER_CLIENT_SCREEN("modifiers", [] { return std::make_unique<bs::client::screens::edit::ModifiersScreen>(); });

#endif // BS_CLIENT
