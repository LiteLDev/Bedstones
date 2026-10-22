#pragma once

// Java-Edition-like GUI look (original procedural pixel art, no Mojang textures): the dimmed
// screen background, vanilla-style buttons, checkboxes, scrollbars, tooltips and the colors of
// ChatFormatting. Waystones' screens are built from vanilla widgets, so these are its look too.

#include <cstdint>
#include <string>
#include <string_view>

#include "client/ui/gfx/Painter.h"

namespace bs::client::ui::style {

// ChatFormatting colors (0xRRGGBB).
inline constexpr std::uint32_t kBlack       = 0x000000;
inline constexpr std::uint32_t kDarkBlue    = 0x0000AA;
inline constexpr std::uint32_t kDarkGreen   = 0x00AA00;
inline constexpr std::uint32_t kDarkAqua    = 0x00AAAA;
inline constexpr std::uint32_t kDarkRed     = 0xAA0000;
inline constexpr std::uint32_t kDarkPurple  = 0xAA00AA;
inline constexpr std::uint32_t kGold        = 0xFFAA00;
inline constexpr std::uint32_t kGray        = 0xAAAAAA;
inline constexpr std::uint32_t kDarkGray    = 0x555555;
inline constexpr std::uint32_t kBlue        = 0x5555FF;
inline constexpr std::uint32_t kGreen       = 0x55FF55;
inline constexpr std::uint32_t kAqua        = 0x55FFFF;
inline constexpr std::uint32_t kRed         = 0xFF5555;
inline constexpr std::uint32_t kLightPurple = 0xFF55FF;
inline constexpr std::uint32_t kYellow      = 0xFFFF55;
inline constexpr std::uint32_t kWhite       = 0xFFFFFF;
/// Vanilla GUI text colors.
inline constexpr std::uint32_t kLabel         = 0x404040; ///< container titles
inline constexpr std::uint32_t kButtonText    = 0xFFFFFF;
inline constexpr std::uint32_t kButtonTextOff = 0xA0A0A0;

/// ChatFormatting name ("gold", "dark_aqua", ...) to RGB (white for unknown names).
[[nodiscard]] std::uint32_t formattingColor(std::string_view name);

/// The translucent dark gradient vanilla draws behind screens without a texture.
void screenBackground(Painter& p, Rect const& screen);

/// Vanilla button frame (`widget/button`, `button_highlighted`, `button_disabled` look).
void button(Painter& p, Rect const& r, bool hovered, bool active);
/// Centered button label (shadowed, clipped with "..." when too wide).
void buttonLabel(Painter& p, Rect const& r, std::string const& text, bool active, std::uint32_t rgb = kButtonText);

/// Vanilla checkbox (20x20 box) with a check mark when `checked`.
void checkbox(Painter& p, Rect const& box, bool checked, bool hovered);

/// Scrollbar track + thumb (vanilla `widget/scroller` look). `fraction` = 0..1 scroll position,
/// `visible` = 0..1 visible part of the content.
void scrollbar(Painter& p, Rect const& track, float fraction, float visible);

/// Vanilla tooltip box around content rect `b` (frame drawn outside it).
void tooltipBox(Painter& p, Rect const& b, Color bg, Color top, Color bottom);

/// Thin 1px outline (selected filter buttons, slots).
void outline(Painter& p, Rect const& r, Color c);

/// Text truncated to `maxWidth` with a trailing "..." when needed.
[[nodiscard]] std::string ellipsize(Painter& p, std::string const& text, float maxWidth);

} // namespace bs::client::ui::style
