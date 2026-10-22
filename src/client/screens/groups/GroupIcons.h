#pragma once

// Group icons (original 16x16 pixel art) and the small sprites of the group screens.
// Other screens that show groups (selection filters, personal settings) can draw the same icons
// with `drawGroupIcon`.

#ifdef BS_CLIENT

#include <string_view>

#include "client/ui/gfx/Painter.h"

namespace bs::client::screens::groups {

/// Draws a group icon ("favorites", "the_nether", ...; unknown ids draw "dimension") at (x, y),
/// 16x16 * scale.
void drawGroupIcon(ui::Painter& p, std::string_view icon, float x, float y, float scale = 1, float alpha = 1);

// 13x13 row sprites.
void drawEditSprite(ui::Painter& p, float x, float y, bool hovered);
void drawDeleteSprite(ui::Painter& p, float x, float y, bool active, bool hovered);
void drawEyeSprite(ui::Painter& p, float x, float y, bool hidden, bool hovered);
/// 16x20 drag handle.
void drawDragHandle(ui::Painter& p, float x, float y, bool hovered);
/// 16x16 "back" arrow.
void drawBackSprite(ui::Painter& p, float x, float y);

} // namespace bs::client::screens::groups

#endif
