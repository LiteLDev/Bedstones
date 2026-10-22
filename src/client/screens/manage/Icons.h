#pragma once

// Small original pixel-art icons of the manage screens (client build). Drawn with
// Painter::pixels; `X` is the main colour, other letters are fixed accents.

#ifdef BS_CLIENT

#include <string>
#include <vector>

#include "client/ui/UiLayer.h"
#include "client/ui/gfx/Painter.h"
#include "client/ui/widgets/Style.h"

namespace bs::client::screens::manage::icons {

using ui::Color;
using ui::Painter;
using ui::PixelArt;
using ui::Rect;

/// Draws `art` centred in `r` with `X` (and every palette entry) at `alpha`.
inline void centered(Painter& p, PixelArt const& art, Rect const& r, float alpha = 1) {
    p.pixels(art, r.x + (r.w - static_cast<float>(art.width())) / 2, r.y + (r.h - static_cast<float>(art.height())) / 2, 1, Painter::Layer::Foreground, alpha);
}

// Back arrow (left column, 20x20 button).
inline PixelArt const kBack{
    {
        "....W.......",
        "...WW.......",
        "..WWWWWWWWWW",
        ".WWWWWWWWWWW",
        "..WWWWWWWWWW",
        "...WW.......",
        "....W.......",
    },
    {"W=FFFFFF"},
};

// Manage Groups: a stack of three labels.
inline PixelArt const kGroups{
    {
        "GGGG........",
        "GGGGGGGGGGGG",
        "............",
        "YYYY........",
        "YYYYYYYYYYYY",
        "............",
        "AAAA........",
        "AAAAAAAAAAAA",
    },
    {"G=FFAA00", "Y=55FF55", "A=55FFFF"},
};

// Drag handle: two columns of grip dots.
inline PixelArt const kGrip{
    {
        "XX.XX",
        "XX.XX",
        ".....",
        "XX.XX",
        "XX.XX",
        ".....",
        "XX.XX",
        "XX.XX",
    },
    {"X=A0A0A0"},
};
inline PixelArt const kGripHover{kGrip.rows, {"X=FFFFFF"}};

// Personal Settings / Edit: a pencil.
inline PixelArt const kPencil{
    {
        ".......EE",
        "......EEE",
        ".....YYE.",
        "....YYY..",
        "...YYY...",
        "..YYY....",
        ".WYY.....",
        "WWW......",
        "K........",
    },
    {"E=E07070", "Y=F0C040", "W=E8D0A0", "K=404040"},
};

// Delete X (bright while Shift is held).
inline PixelArt const kDelete{
    {
        "RR.....RR",
        "RRR...RRR",
        ".RRR.RRR.",
        "..RRRRR..",
        "...RRR...",
        "..RRRRR..",
        ".RRR.RRR.",
        "RRR...RRR",
        "RR.....RR",
    },
    {"R=FF5555"},
};
inline PixelArt const kDeleteOff{kDelete.rows, {"R=6A3A3A"}};
inline PixelArt const kDeleteHover{kDelete.rows, {"R=FF9090"}};

// Eye toggle: open (shown) and closed (hidden).
inline PixelArt const kEye{
    {
        "..XXXXXXX..",
        ".X.......X.",
        "X...KKK...X",
        "X..KKWKK..X",
        "X...KKK...X",
        ".X.......X.",
        "..XXXXXXX..",
    },
    {"X=E0E0E0", "K=3060C0", "W=FFFFFF"},
};
inline PixelArt const kEyeClosed{
    {
        "...........",
        "...........",
        "X.........X",
        ".X.......X.",
        "..XXXXXXX..",
        ".X..X.X..X.",
        "...........",
    },
    {"X=A0A0A0"},
};

/// A group label tag in the group's colour (cycle button, 16x16 slot).
inline void groupTag(Painter& p, Rect const& r, std::uint32_t rgb) {
    float x = r.x + 2, y = r.y + 4;
    auto  c = Color::rgb(rgb);
    p.fillRect({x + 2, y, 10, 8}, c, Painter::Layer::Foreground);
    p.fillRect({x + 1, y + 1, 1, 6}, c, Painter::Layer::Foreground);
    p.fillRect({x, y + 2, 1, 4}, c, Painter::Layer::Foreground);
    p.fillRect({x + 3, y + 3, 2, 2}, Color::rgb(0x202020), Painter::Layer::Foreground);
}

/// Drops repeated presses / releases of a mouse button (some input paths deliver a press twice,
/// which would read as a double-click on the drag handle).
struct ButtonLatch {
    bool held[3]{};
    /// False when the event repeats the button's current state.
    bool accept(ui::MouseButton b, bool down) {
        auto& h = held[static_cast<int>(b)];
        if (h == down) return false;
        h = down;
        return true;
    }
};

/// Splits `text` into lines no wider than `maxWidth` (tooltips; Java wraps at 170 px).
inline std::vector<std::string> wrap(Painter& p, std::string const& text, float maxWidth = 170) {
    std::vector<std::string> lines;
    std::string              line;
    std::size_t              i = 0;
    while (i < text.size()) {
        auto end  = text.find(' ', i);
        auto word = text.substr(i, end == std::string::npos ? std::string::npos : end - i);
        i         = end == std::string::npos ? text.size() : end + 1;
        auto next = line.empty() ? word : line + " " + word;
        if (!line.empty() && p.textWidth(next) > maxWidth) {
            lines.push_back(line);
            line = word;
        } else {
            line = next;
        }
    }
    if (!line.empty()) lines.push_back(line);
    return lines;
}

} // namespace bs::client::screens::manage::icons

#endif // BS_CLIENT
