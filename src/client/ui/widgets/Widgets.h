#pragma once

// Vanilla-like widgets for the native Waystones screens (client thread only).
//
// Widgets are plain structs owned by a screen. The screen lays them out (sets `rect`), calls
// `render` every frame and forwards input; nothing is retained by the framework. Coordinates are
// GUI units. Tooltips are collected with `hoverTooltip` and drawn last by the screen.

#include <functional>
#include <string>
#include <vector>

#include "client/ui/UiLayer.h"
#include "client/ui/gfx/Painter.h"
#include "client/ui/widgets/Tooltip.h"

namespace bs::client::ui {

/// A vanilla button. With `icon` set it draws the icon centered instead of (or left of) the label.
struct Button {
    Rect                                            rect;
    std::string                                     label;
    std::uint32_t                                   labelColor = 0xFFFFFF;
    bool                                            active     = true;
    bool                                            visible    = true;
    bool                                            frame      = true;  ///< draw the button background
    std::function<void(Painter&, Rect const&, bool hovered)> icon;     ///< optional icon painter
    std::vector<TooltipLine>                        tooltip;
    std::function<void()>                           onPress;      ///< left click
    std::function<void()>                           onRightPress; ///< right click (cycle backwards, ...)

    void render(Painter& p, float mx, float my) const;
    /// Returns true when the click hit the button (consumed even if inactive).
    bool click(MouseButton button, float x, float y) const;
    [[nodiscard]] bool hovered(float mx, float my) const { return visible && rect.contains(mx, my); }
};

/// Vanilla checkbox with a label to its right.
struct Checkbox {
    Rect                       rect; ///< the 20x20 box (label drawn to the right)
    std::string                label;
    bool                       checked = false;
    bool                       active  = true;
    std::function<void(bool)>  onChange;

    void render(Painter& p, float mx, float my) const;
    bool click(MouseButton button, float x, float y);
};

/// Vertical list with fixed-height rows, wheel scrolling and a draggable scrollbar
/// (vanilla `AbstractSelectionList` behaviour). Rows draw themselves through `drawRow`.
struct ScrollList {
    Rect  rect;            ///< list area (rows are clipped by skipping rows outside it)
    float rowHeight = 22;
    int   rowCount  = 0;
    float scroll    = 0;   ///< pixels scrolled from the top
    bool  scrollbarVisible = true;
    float scrollbarWidth   = 6;

    /// (painter, row index, row rect, hovered)
    std::function<void(Painter&, int, Rect const&, bool)> drawRow;
    /// (row index, button, x, y) -> consumed
    std::function<bool(int, MouseButton, float, float)>   clickRow;

    void render(Painter& p, float mx, float my);
    bool click(MouseButton button, bool down, float x, float y);
    bool drag(float x, float y);
    bool wheel(float x, float y, float delta);
    /// Row index under (x, y), -1 if none.
    [[nodiscard]] int   rowAt(float x, float y) const;
    [[nodiscard]] Rect  rowRect(int index) const;
    [[nodiscard]] float maxScroll() const;
    void                clampScroll();
    /// Scrolls so that `index` is visible.
    void ensureVisible(int index);

private:
    bool  mDragging = false;
    float mDragOffset = 0;
    [[nodiscard]] Rect track() const;
};

/// Collects the tooltip of whatever is hovered this frame; the screen draws it last.
struct HoverTooltip {
    std::vector<TooltipLine> lines;
    float                    x = 0, y = 0;
    void set(std::vector<TooltipLine> l, float mx, float my) {
        lines = std::move(l);
        x     = mx;
        y     = my;
    }
    void clear() { lines.clear(); }
    void draw(Painter& p) const;
};

} // namespace bs::client::ui
