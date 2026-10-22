#ifdef BS_CLIENT

#include "client/ui/widgets/Widgets.h"

#include <algorithm>
#include <cmath>

#include "client/ui/widgets/Style.h"

namespace bs::client::ui {

// ---- Button ---------------------------------------------------------------------------------------

void Button::render(Painter& p, float mx, float my) const {
    if (!visible) return;
    bool hov = active && rect.contains(mx, my);
    if (frame) style::button(p, rect, hov, active);
    if (icon) {
        Rect iconRect = label.empty() ? rect : Rect{rect.x + 2, rect.y + (rect.h - 16) / 2, 16, 16};
        icon(p, iconRect, hov);
    }
    if (!label.empty()) {
        Rect textRect = icon ? Rect{rect.x + 18, rect.y, rect.w - 18, rect.h} : rect;
        style::buttonLabel(p, textRect, label, active, labelColor);
    }
}

bool Button::click(MouseButton button, float x, float y) const {
    if (!visible || !rect.contains(x, y)) return false;
    if (!active) return true;
    if (button == MouseButton::Left && onPress) onPress();
    else if (button == MouseButton::Right && onRightPress) onRightPress();
    return true;
}

// ---- Checkbox -------------------------------------------------------------------------------------

void Checkbox::render(Painter& p, float mx, float my) const {
    bool hov = active && rect.contains(mx, my);
    style::checkbox(p, rect, checked, hov);
    if (!label.empty()) {
        p.text(label, rect.right() + 4, rect.y + (rect.h - 8) / 2, Color::rgb(active ? 0xE0E0E0 : 0xA0A0A0), true);
    }
}

bool Checkbox::click(MouseButton button, float x, float y) {
    float labelW = label.empty() ? 0 : 4 + 9.f * static_cast<float>(label.size()) * 0.6f;
    Rect  hit{rect.x, rect.y, rect.w + labelW, rect.h};
    if (button != MouseButton::Left || !hit.contains(x, y)) return false;
    if (!active) return true;
    checked = !checked;
    if (onChange) onChange(checked);
    return true;
}

// ---- ScrollList -----------------------------------------------------------------------------------

Rect ScrollList::track() const { return {rect.right() - scrollbarWidth, rect.y, scrollbarWidth, rect.h}; }

float ScrollList::maxScroll() const { return std::max(0.f, rowHeight * static_cast<float>(rowCount) - rect.h); }

void ScrollList::clampScroll() { scroll = std::clamp(scroll, 0.f, maxScroll()); }

Rect ScrollList::rowRect(int index) const {
    return {rect.x, rect.y + rowHeight * static_cast<float>(index) - scroll, rect.w - (scrollbarVisible && maxScroll() > 0 ? scrollbarWidth + 2 : 0), rowHeight};
}

int ScrollList::rowAt(float x, float y) const {
    if (!rect.contains(x, y)) return -1;
    int idx = static_cast<int>(std::floor((y - rect.y + scroll) / rowHeight));
    if (idx < 0 || idx >= rowCount) return -1;
    return rowRect(idx).contains(x, y) ? idx : -1;
}

void ScrollList::ensureVisible(int index) {
    float top = rowHeight * static_cast<float>(index);
    if (top < scroll) scroll = top;
    else if (top + rowHeight > scroll + rect.h) scroll = top + rowHeight - rect.h;
    clampScroll();
}

void ScrollList::render(Painter& p, float mx, float my) {
    clampScroll();
    if (drawRow) {
        int first = std::max(0, static_cast<int>(std::floor(scroll / rowHeight)));
        for (int i = first; i < rowCount; ++i) {
            Rect r = rowRect(i);
            if (r.y >= rect.bottom()) break;
            // Rows partly outside are skipped (the painter has no scissor): like a list that
            // snaps rows to its edges.
            if (r.y < rect.y - 0.5f || r.bottom() > rect.bottom() + 0.5f) continue;
            drawRow(p, i, r, r.contains(mx, my));
        }
    }
    if (scrollbarVisible && maxScroll() > 0) {
        style::scrollbar(p, track(), maxScroll() > 0 ? scroll / maxScroll() : 0, rect.h / (rowHeight * static_cast<float>(rowCount)));
    }
}

bool ScrollList::click(MouseButton button, bool down, float x, float y) {
    if (!down) {
        bool was  = mDragging;
        mDragging = false;
        return was;
    }
    if (scrollbarVisible && maxScroll() > 0 && button == MouseButton::Left && track().contains(x, y)) {
        mDragging   = true;
        mDragOffset = 0;
        drag(x, y);
        return true;
    }
    int idx = rowAt(x, y);
    if (idx < 0) return rect.contains(x, y);
    if (clickRow) return clickRow(idx, button, x, y);
    return true;
}

bool ScrollList::drag(float, float y) {
    if (!mDragging) return false;
    Rect  t       = track();
    float visible = std::clamp(rect.h / (rowHeight * static_cast<float>(std::max(rowCount, 1))), 0.05f, 1.f);
    float thumbH  = std::max(8.f, t.h * visible);
    float range   = t.h - thumbH;
    if (range <= 0) return true;
    scroll = (y - t.y - thumbH / 2) / range * maxScroll();
    clampScroll();
    return true;
}

bool ScrollList::wheel(float x, float y, float delta) {
    if (!rect.contains(x, y) || maxScroll() <= 0) return false;
    scroll -= delta * rowHeight / 2;
    clampScroll();
    return true;
}

void HoverTooltip::draw(Painter& p) const {
    if (lines.empty()) return;
    drawTooltip(p, lines, x, y, nullptr, {}, false);
}

} // namespace bs::client::ui

#endif // BS_CLIENT
