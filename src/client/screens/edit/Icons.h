#pragma once

// Original pixel-art sprites of the edit / modifiers screens (client build): visibility icons,
// the personal settings and modifiers button icons and Standard-Galactic-like glyphs.

#include <string_view>

#include "client/ui/gfx/Painter.h"

namespace bs::client::screens::edit::icons {

using ui::PixelArt;

// Eye: visible to whoever activated it.
inline PixelArt const kActivation{
    {
        "....XXXXXX....",
        "..XXwwwwwwXX..",
        ".XwwwwBBwwwwX.",
        "XwwwwBBBBwwwwX",
        "XwwwBBkkBBwwwX",
        "XwwwBBkkBBwwwX",
        "XwwwwBBBBwwwwX",
        ".XwwwwBBwwwwX.",
        "..XXwwwwwwXX..",
        "....XXXXXX....",
    },
    {"X=1E1E1E", "w=F2F2F2", "B=3C8FD6", "k=10202E"},
};

// Globe: visible to everyone.
inline PixelArt const kGlobal{
    {
        "....XXXXX....",
        "..XXbbgbbXX..",
        ".XbggbbggbbX.",
        ".XggggbbgbbX.",
        "XbbgggbbbbbbX",
        "XbbbggbbbggbX",
        "XbbbbbbbgggbX",
        "XbbbbbbggggbX",
        ".XbbbbbbggbX.",
        ".XbgbbbbbbbX.",
        "..XXbggbbXX..",
        "....XXXXX....",
    },
    {"X=1A2A44", "b=3D7BD9", "g=4FB04A"},
};

// Two heads: visible to the owner's team.
inline PixelArt const kTeam{
    {
        "..XXX.....XXX..",
        ".XsssX...XsssX.",
        ".XsssX...XsssX.",
        "..XXX.....XXX..",
        ".XbbbX...XrrrX.",
        "XbbbbbX.XrrrrrX",
        "XbbbbbX.XrrrrrX",
        "XbbbbbX.XrrrrrX",
        "XXXXXXX.XXXXXXX",
    },
    {"X=1E1E1E", "s=E8B48A", "b=3F6FD8", "r=D84A3F"},
};

// Attuned shard: teleports via an attuned shard (warp plates).
inline PixelArt const kShardOnly{
    {
        ".....XX.....",
        "....XmmX....",
        "...XmllmX...",
        "...XmlmmX...",
        "..XmmlmpmX..",
        "..XmllmppX..",
        ".XmmlmmpmX..",
        ".XmlmmppmX..",
        "..XmmppmX...",
        "...XmpmX....",
        "....XXX.....",
    },
    {"X=2A103A", "m=A24BD6", "l=E3B6FF", "p=6E2A9C"},
};

// Small two-tall stone; tinted with the sharestone's dye colour (the 'c' runes).
inline PixelArt const kSharestone{
    {
        "...XXXXX...",
        "..XsssssX..",
        "..XscscsX..",
        "..XsssssX..",
        "..XscccsX..",
        "..XsssssX..",
        "..XsscssX..",
        "..XsssssX..",
        ".XXXXXXXXX.",
        ".XdddddddX.",
        ".XXXXXXXXX.",
    },
    {"X=202020", "s=8A8A8A", "d=5A5A5A", "c=FFFFFF"},
};

// Head and shoulders with a small gear: personal settings.
inline PixelArt const kPersonal{
    {
        "...XXXX.......",
        "..XssssX......",
        "..XssssX......",
        "..XssssX......",
        "...XXXX.......",
        ".XXbbbbXX.....",
        "XbbbbbbbbX....",
        "XbbbbbbXX.g.g.",
        "XbbbbbX..ggggg",
        "XbbbbbX...gXg.",
        "XXXXXXX..ggggg",
        "..........g.g.",
    },
    {"X=1E1E1E", "s=E8B48A", "b=4C9A57", "g=D0D0D0"},
};

// Diamond over a small cross of slots: manage modifiers.
inline PixelArt const kModifiers{
    {
        "....XXXX....",
        "...XaaaaX...",
        "..XalaaaaX..",
        ".XallaaaadX.",
        ".XXXXXXXXXX.",
        "..XaaaadX...",
        "...XaadX....",
        "....XadX....",
        ".....XX.....",
    },
    {"X=113A3A", "a=4FE0D8", "l=D8FFFC", "d=239C96"},
};

/// Glyphs for the six letters of a hex uid (a-f), Standard-Galactic-like (3x5).
inline PixelArt const& galacticGlyph(char c) {
    static PixelArt const kA{{"..X", "..X", "XXX", "X..", "XXX"}, {"X=FFFFFF"}};
    static PixelArt const kB{{".X.", ".X.", ".X.", "X.X", ".X."}, {"X=FFFFFF"}};
    static PixelArt const kC{{"XXX", "X..", "X.X", "X..", "XXX"}, {"X=FFFFFF"}};
    static PixelArt const kD{{"XXX", "X..", "X.X", ".XX", "XXX"}, {"X=FFFFFF"}};
    static PixelArt const kE{{"X.X", "X.X", "XXX", ".X.", ".X."}, {"X=FFFFFF"}};
    static PixelArt const kF{{"XXX", "...", "X.X", "...", "XXX"}, {"X=FFFFFF"}};
    switch (c) {
    case 'a': return kA;
    case 'b': return kB;
    case 'c': return kC;
    case 'd': return kD;
    case 'e': return kE;
    default: return kF;
    }
}

} // namespace bs::client::screens::edit::icons
