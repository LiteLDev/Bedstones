#pragma once

// Original pixel art of the selection screen (16x16 unless noted). Drawn with Painter::pixels.

#include "client/ui/gfx/Painter.h"

namespace bs::client::screens::selection::sprites {

using ui::PixelArt;

// Enchanting-table-like level orb; the 1/2/3 marks are drawn on top by the screen.
inline PixelArt const kLevelOrb{
    {
        "................",
        "......OOOO......",
        "....OOGGGGOO....",
        "...OGGLLGGGGO...",
        "..OGGLGGGGGGGO..",
        "..OGLGGGGGGGGO..",
        ".OGGGGGGGGGGGGO.",
        ".OGGGGGGGGGGGGO.",
        ".OGGGGGGGGGGGGO.",
        ".OGGGGGGGGGGGGO.",
        "..OGGGGGGGGGDO..",
        "..OGGGGGGGGDDO..",
        "...ODGGGGGDDO...",
        "....OODDDDOO....",
        "......OOOO......",
        "................",
    },
    {"O=1E5E1E", "G=3FBF3F", "L=9FFF8F", "D=2A8A2A"},
};
inline PixelArt const kLevelOrbDisabled{
    kLevelOrb.rows,
    {"O=303030", "G=6E6E6E", "L=A0A0A0", "D=505050"},
};

// Refused warp (Waystones' cancel sprite).
inline PixelArt const kCancel{
    {
        "................",
        ".....RRRRRR.....",
        "...RRrrrrrrRR...",
        "..RrrrrrrrrrrR..",
        "..RrWWrrrrWWrR..",
        ".RrrrWWrrWWrrrR.",
        ".RrrrrWWWWrrrrR.",
        ".RrrrrrWWrrrrrR.",
        ".RrrrrWWWWrrrrR.",
        ".RrrrWWrrWWrrrR.",
        "..RrWWrrrrWWrR..",
        "..RrrrrrrrrrrR..",
        "...RRrrrrrrRR...",
        ".....RRRRRR.....",
        "................",
        "................",
    },
    {"R=5A0A0A", "r=D02828", "W=FFFFFF"},
};

// Pencil shown next to the hovered "Current Location" name.
inline PixelArt const kEdit{
    {
        "................",
        "............kk..",
        "...........kppk.",
        "..........kpppk.",
        ".........kYYpk..",
        "........kYYYk...",
        ".......kYYYk....",
        "......kYYYk.....",
        ".....kYYYk......",
        "....kYYYk.......",
        "...kYYYk........",
        "...kwYk.........",
        "..kwwk..........",
        "..kgk...........",
        "..kk............",
        "................",
    },
    {"k=1E1E1E", "p=E86A8A", "Y=F2C14E", "w=E8D2A8", "g=404040"},
};

// Sort modes.
inline PixelArt const kSortManual{
    {
        "................",
        "................",
        "..gg.WWWWWWWWW..",
        "..gg.WWWWWWWWW..",
        "................",
        "..gg.WWWWWWW....",
        "..gg.WWWWWWW....",
        "................",
        "..gg.WWWWWWWWW..",
        "..gg.WWWWWWWWW..",
        "................",
        "..gg.WWWWWW.....",
        "..gg.WWWWWW.....",
        "................",
        "................",
        "................",
    },
    {"g=909090", "W=E0E0E0"},
};
inline PixelArt const kSortName{
    {
        "................",
        "................",
        "..WWW.......a...",
        ".W...W......a...",
        ".WWWWW......a...",
        ".W...W......a...",
        ".W...W......a...",
        "............a...",
        ".WWWWW......a...",
        "....W.......a...",
        "...W......aaaaa.",
        "..W........aaa..",
        ".WWWWW......a...",
        "................",
        "................",
        "................",
    },
    {"W=E0E0E0", "a=FFE070"},
};
inline PixelArt const kSortDistance{
    {
        "................",
        "................",
        "................",
        "..a.........a...",
        ".aa.........aa..",
        "aaaaaaaaaaaaaaa.",
        ".aa.........aa..",
        "..a.........a...",
        "................",
        ".RRRRRRRRRRRRRR.",
        ".RrRrRrRrRrRrRR.",
        ".RrrrrrrrrrrrrR.",
        ".RRRRRRRRRRRRRR.",
        "................",
        "................",
        "................",
    },
    {"a=E0E0E0", "R=C8A060", "r=7A5A2A"},
};

// "Manage Waystones": a small waystone.
inline PixelArt const kManage{
    {
        "................",
        "......kkkk......",
        ".....kSSSSk.....",
        ".....kSssSk.....",
        "....kSSSSSSk....",
        "....kSSmmSSk....",
        "....kSmSSmSk....",
        "....kSSmmSSk....",
        "....kSSSSSSk....",
        "....kSSmmSSk....",
        "....kSmSSmSk....",
        "....kSSmmSSk....",
        "...kSSSSSSSSk...",
        "..kSSSSSSSSSSk..",
        "..kkkkkkkkkkkk..",
        "................",
    },
    {"k=2A2A2A", "S=9A9A9A", "s=C8C8C8", "m=B04CE0"},
};

// Group icons.
inline PixelArt const kStar{
    {
        "................",
        ".......oo.......",
        ".......oo.......",
        "......oYYo......",
        "......oYYo......",
        ".ooooooYYoooooo.",
        "..oYYYYYYYYYYo..",
        "...oYYYYYYYYo...",
        "....oYYYYYYo....",
        "....oYYYYYYo....",
        "...oYYYooYYYo...",
        "...oYYo..oYYo...",
        "..oYo......oYo..",
        "..oo........oo..",
        "................",
        "................",
    },
    {"o=8A5A00", "Y=FFC800"},
};
inline PixelArt const kPlayer{
    {
        "................",
        "................",
        "....kkkkkkkk....",
        "....kHHHHHHk....",
        "....kHHHHHHk....",
        "....kFFFFFFk....",
        "....kFeFFeFk....",
        "....kFFFFFFk....",
        "....kFFmmFFk....",
        "....kkkkkkkk....",
        "...kSSSSSSSSk...",
        "..kSSSSSSSSSSk..",
        "..kSSSSSSSSSSk..",
        "..kSSSSSSSSSSk..",
        "..kkkkkkkkkkkk..",
        "................",
    },
    {"k=1E1E1E", "H=5A3A1A", "F=E0A878", "e=3050C0", "m=A05040", "S=30A0A0"},
};
inline PixelArt const kTeamMate{
    kPlayer.rows,
    {"k=1E1E1E", "H=C8A040", "F=D89868", "e=3A8A3A", "m=A05040", "S=C04848"},
};
inline PixelArt const kGlobe{
    {
        "................",
        ".....kkkkkk.....",
        "...kkBBGGBBkk...",
        "..kBBGGGGBBBBk..",
        ".kBBBGGGBBBBBBk.",
        ".kBBGGGBBBBGGBk.",
        "kBBBBGGBBBGGGGBk",
        "kBBBBBBBBBGGGGBk",
        "kBBBBBBBBGGGGBBk",
        "kBBGGBBBBBGGBBBk",
        ".kBGGGBBBBBBBBk.",
        ".kBBGGGBBBBBBBk.",
        "..kBBGGBBBBBBk..",
        "...kkBBBBBBkk...",
        ".....kkkkkk.....",
        "................",
    },
    {"k=1A2A5A", "B=3A7AE0", "G=4CB04C"},
};

// Dimension groups: a portal frame tinted per dimension.
inline constexpr std::string_view kPortalRows[]{
    "................",
    "...oooooooooo...",
    "...oPPPPPPPPo...",
    "...oPpPPPPpPo...",
    "...oPPPpPPPPo...",
    "...oPPPPPPpPo...",
    "...opPPPPPPPo...",
    "...oPPPpPPPPo...",
    "...oPPPPPpPPo...",
    "...oPpPPPPPPo...",
    "...oPPPPpPPPo...",
    "...oPPPPPPPpo...",
    "...oPPpPPPPPo...",
    "...oPPPPPPPPo...",
    "...oooooooooo...",
    "................",
};
inline PixelArt const kOverworld{{std::begin(kPortalRows), std::end(kPortalRows)}, {"o=2A1E3A", "P=4C9A34", "p=8FD06F"}};
inline PixelArt const kNether{{std::begin(kPortalRows), std::end(kPortalRows)}, {"o=2A1E3A", "P=8A1E1E", "p=E05030"}};
inline PixelArt const kEnd{{std::begin(kPortalRows), std::end(kPortalRows)}, {"o=2A1E3A", "P=14142A", "p=E8E8A0"}};

// User groups: a tag in the group's colour ('C' is replaced at runtime).
inline constexpr std::string_view kTagRows[]{
    "................",
    "................",
    "..kkkkkkkk......",
    "..kCCCCCCCk.....",
    "..kCwwCCCCCk....",
    "..kCwwCCCCCCk...",
    "..kCCCCCCCCCCk..",
    "..kCCCCCCCCCCCk.",
    "..kCCCCCCCCCCk..",
    "..kCCCCCCCCCk...",
    "..kCCCCCCCCk....",
    "..kCCCCCCCk.....",
    "..kkkkkkkk......",
    "................",
    "................",
    "................",
};

} // namespace bs::client::screens::selection::sprites
