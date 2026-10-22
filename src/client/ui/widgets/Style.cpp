#ifdef BS_CLIENT

#include "client/ui/widgets/Style.h"

#include <algorithm>
#include <array>
#include <utility>

namespace bs::client::ui::style {

using L = Painter::Layer;

std::uint32_t formattingColor(std::string_view name) {
    static constexpr std::array<std::pair<std::string_view, std::uint32_t>, 16> kTable{{
        {"black", kBlack},
        {"dark_blue", kDarkBlue},
        {"dark_green", kDarkGreen},
        {"dark_aqua", kDarkAqua},
        {"dark_red", kDarkRed},
        {"dark_purple", kDarkPurple},
        {"gold", kGold},
        {"gray", kGray},
        {"dark_gray", kDarkGray},
        {"blue", kBlue},
        {"green", kGreen},
        {"aqua", kAqua},
        {"red", kRed},
        {"light_purple", kLightPurple},
        {"yellow", kYellow},
        {"white", kWhite},
    }};
    for (auto const& [n, c] : kTable) {
        if (n == name) return c;
    }
    return kWhite;
}

void screenBackground(Painter& p, Rect const& screen) {
    p.gradientRect(screen, Color::argb(0xC0101010), Color::argb(0xD0101010));
}

void button(Painter& p, Rect const& r, bool hovered, bool active) {
    if (r.empty()) return;
    if (!active) {
        p.fillRect(r, Color::argb(0xFF2B2B2B));
        p.fillRect({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, Color::argb(0xFF3C3C3C));
        return;
    }
    // Outer border: black, or white when hovered (1.20.2+ highlight).
    p.fillRect(r, hovered ? Color::argb(0xFFFFFFFF) : Color::argb(0xFF000000));
    Rect in{r.x + 1, r.y + 1, r.w - 2, r.h - 2};
    // Stone-gray face with a light top-left bevel and a dark bottom-right bevel.
    p.fillRect(in, Color::argb(0xFF6F6F6F));
    p.fillRect({in.x, in.y, in.w, 1}, Color::argb(0xFFA9A9A9));
    p.fillRect({in.x, in.y, 1, in.h}, Color::argb(0xFFA9A9A9));
    p.fillRect({in.x, in.bottom() - 2, in.w, 2}, Color::argb(0xFF4D4D4D));
    p.fillRect({in.right() - 1, in.y + 1, 1, in.h - 1}, Color::argb(0xFF575757));
    if (hovered) p.fillRect({in.x + 1, in.y + 1, in.w - 2, in.h - 3}, Color::argb(0x307F8FFF));
}

void buttonLabel(Painter& p, Rect const& r, std::string const& text, bool active, std::uint32_t rgb) {
    auto shown = ellipsize(p, text, r.w - 6);
    float y    = r.y + (r.h - 8) / 2;
    p.text(shown, r.x, y, Color::rgb(active ? rgb : kButtonTextOff), true, 1, TextAlign::Center, r.w);
}

void checkbox(Painter& p, Rect const& box, bool checked, bool hovered) {
    p.fillRect(box, hovered ? Color::argb(0xFFFFFFFF) : Color::argb(0xFF000000));
    Rect in = box.inset(1);
    p.fillRect(in, Color::argb(0xFF8B8B8B));
    p.fillRect({in.x, in.y, in.w, 1}, Color::argb(0xFF373737));
    p.fillRect({in.x, in.y, 1, in.h}, Color::argb(0xFF373737));
    p.fillRect({in.x, in.bottom() - 1, in.w, 1}, Color::argb(0xFFFFFFFF));
    p.fillRect({in.right() - 1, in.y, 1, in.h}, Color::argb(0xFFFFFFFF));
    if (!checked) return;
    // Check mark (original pixel art).
    static PixelArt const kCheck{
        {
            "..........X",
            ".........XX",
            "........XX.",
            "X......XX..",
            "XX....XX...",
            ".XX..XX....",
            "..XXXX.....",
            "...XX......",
        },
        {"X=FFFFFF"},
    };
    float scale = std::max(1.f, (in.w - 6) / 11.f);
    p.pixels(kCheck, in.x + (in.w - 11 * scale) / 2, in.y + (in.h - 8 * scale) / 2, scale, L::Foreground);
}

void scrollbar(Painter& p, Rect const& track, float fraction, float visible) {
    p.fillRect(track, Color::argb(0xFF000000));
    visible       = std::clamp(visible, 0.05f, 1.f);
    float thumbH  = std::max(8.f, track.h * visible);
    float thumbY  = track.y + (track.h - thumbH) * std::clamp(fraction, 0.f, 1.f);
    Rect  thumb{track.x, thumbY, track.w, thumbH};
    p.fillRect(thumb, Color::argb(0xFF808080));
    p.fillRect({thumb.x, thumb.y, thumb.w - 1, thumb.h - 1}, Color::argb(0xFFC0C0C0));
}

void tooltipBox(Painter& p, Rect const& b, Color bg, Color top, Color bottom) {
    auto fg = L::Foreground;
    p.fillRect({b.x - 3, b.y - 4, b.w + 6, 1}, bg, fg);
    p.fillRect({b.x - 3, b.bottom() + 3, b.w + 6, 1}, bg, fg);
    p.fillRect({b.x - 3, b.y - 3, b.w + 6, b.h + 6}, bg, fg);
    p.fillRect({b.x - 4, b.y - 3, 1, b.h + 6}, bg, fg);
    p.fillRect({b.right() + 3, b.y - 3, 1, b.h + 6}, bg, fg);
    p.fillRect({b.x - 3, b.y - 3, b.w + 6, 1}, top, fg);
    p.fillRect({b.x - 3, b.bottom() + 2, b.w + 6, 1}, bottom, fg);
    float sideTop = b.y - 2, sideH = b.h + 4;
    int   steps   = std::max(1, static_cast<int>(sideH / 2));
    for (int i = 0; i < steps; ++i) {
        float y0 = sideTop + sideH * static_cast<float>(i) / static_cast<float>(steps);
        float y1 = sideTop + sideH * static_cast<float>(i + 1) / static_cast<float>(steps);
        float t  = (static_cast<float>(i) + 0.5f) / static_cast<float>(steps);
        Color c{top.r + (bottom.r - top.r) * t, top.g + (bottom.g - top.g) * t, top.b + (bottom.b - top.b) * t, top.a + (bottom.a - top.a) * t};
        p.fillRect({b.x - 3, y0, 1, y1 - y0}, c, fg);
        p.fillRect({b.right() + 2, y0, 1, y1 - y0}, c, fg);
    }
}

void outline(Painter& p, Rect const& r, Color c) { p.outlineRect(r, c, 1, L::Foreground); }

std::string ellipsize(Painter& p, std::string const& text, float maxWidth) {
    if (maxWidth <= 0 || p.textWidth(text) <= maxWidth) return text;
    std::string s = text;
    while (!s.empty()) {
        // Drop one UTF-8 code point from the end.
        std::size_t cut = s.size() - 1;
        while (cut > 0 && (static_cast<unsigned char>(s[cut]) & 0xC0) == 0x80) --cut;
        s.erase(cut);
        if (p.textWidth(s + "...") <= maxWidth) return s + "...";
    }
    return "...";
}

} // namespace bs::client::ui::style

#endif // BS_CLIENT
