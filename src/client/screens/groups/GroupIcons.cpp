#ifdef BS_CLIENT

// Group icons and row sprites: original pixel art, drawn with rectangles (no textures).

#include "client/screens/groups/GroupIcons.h"

#include <string_view>

namespace bs::client::screens::groups {

namespace {

// ---- art ------------------------------------------------------------------------------------------

ui::PixelArt const kIconFavorites{
    {
        "................",
        ".......DD.......",
        "......DWYD......",
        "......DWYD......",
        ".....DWYYOD.....",
        "DDDDDDWYYYODDDDD",
        "DWWYYYYYYYYYYYOD",
        ".DYYYYYYYYYYYOD.",
        "..DYYYYYYYYYOD..",
        "...DYYYYYYYOD...",
        "...DYYYYYYYOD...",
        "..DYYYYDDYYYOD..",
        "..DYYYD..DYYOD..",
        ".DYYDD....DDOOD.",
        ".DDD........DDD.",
        "................",
    },
    {"D=8A5A00", "Y=FFD83D", "O=E8A317", "W=FFF4B0"},
};
ui::PixelArt const kIconPlayers{
    {
        "................",
        ".....HHHHHH.....",
        ".....HHHHHH.....",
        ".....HSSSSH.....",
        ".....SKSSKS.....",
        ".....SSSSSS.....",
        ".....SSMMSS.....",
        "...BBBBBBBBBB...",
        "...BBBBBBBBBB...",
        "...SBBBBBBBbS...",
        "...SBBBBBBBbS...",
        "...SLLLLLLLLS...",
        "....LLL..LLL....",
        "....LLL..LLL....",
        "....DDD..DDD....",
        "................",
    },
    {"H=5A3A1E", "S=E0A070", "K=2B2B2B", "M=B06A48", "B=3FA34D", "b=2E7A39", "L=3B4F9E", "D=4A4A4A"},
};
ui::PixelArt const kIconGlobal{
    {
        "................",
        ".....KKKKKK.....",
        "...KKwwLLwwKK...",
        "..KwWWLLLWWWWK..",
        "..KWLLLLlWWWLK..",
        ".KWWLLLLWWWLLLK.",
        ".KWWWLLlWWWLlLK.",
        ".KWWWWLLWWWWLWK.",
        ".KWWWWWLLWWWWWK.",
        ".KWWWWWLLlWWWWK.",
        ".KWLWWWWLLLWWWK.",
        "..KLLWWWWLLWWK..",
        "..KWLlWWWWWWWK..",
        "...KKWWWWWWKK...",
        ".....KKKKKK.....",
        "................",
    },
    {"K=1E3A78", "W=3B6FD6", "w=6A96E8", "L=4FB33F", "l=2F8A2A"},
};
ui::PixelArt const kIconTeams{
    {
        "................",
        "..KKKKKKKKKKKK..",
        "..KAAAAWWAAAaK..",
        "..KAAAAWWAAAaK..",
        "..KAAAAWWAAAaK..",
        "..KWWWWWWWWWWK..",
        "..KWWWWWWWWWWK..",
        "..KAAAAWWAAAaK..",
        "..KAAAAWWAAAaK..",
        "..KAAAAWWAAAaK..",
        "...KAAAWWAAaK...",
        "...KAAAWWAAaK...",
        "....KAAWWAaK....",
        ".....KAWWaK.....",
        "......KKKK......",
        "................",
    },
    {"K=1A3550", "A=2E9BD6", "a=1F6F9E", "W=F2F2F2"},
};
ui::PixelArt const kIconDimension{
    {
        "................",
        "...OOOOOOOOOO...",
        "...OoOOOOOOoO...",
        "...OOPPpPPPOO...",
        "...OOPpPPpPOO...",
        "...OOPPPpPPOO...",
        "...OOpPPPPpOO...",
        "...OOPPpPPPOO...",
        "...OOPPPPpPOO...",
        "...OOPpPPPPOO...",
        "...OOPPPpPPOO...",
        "...OOpPPPPPOO...",
        "...OOPPPpPPOO...",
        "...OoOOOOOOoO...",
        "...OOOOOOOOOO...",
        "................",
    },
    {"O=1B1029", "o=3A2A5A", "P=A23DE0", "p=D27BFF"},
};
ui::PixelArt const kIconOverworld{
    {
        "................",
        ".GGGGGGGGGGGGGG.",
        ".GgGGGGgGGGGgGG.",
        ".GGGGgGGGGgGGGG.",
        ".gGDgGGgDGGgDgG.",
        ".DgDDgDDDgDDDgD.",
        ".DDDDDdDDDDDdDD.",
        ".DdDDDDDDdDDDDD.",
        ".DDDdDDDDDDDdDD.",
        ".DDDDDDdDDDDDDD.",
        ".DdDDDDDDDDdDDD.",
        ".DDDDdDDDdDDDDD.",
        ".DDDDDDDDDDDDdD.",
        ".DdDDDDdDDDDDDD.",
        ".DDDDDDDDDDDDDD.",
        "................",
    },
    {"G=5DA83A", "g=3E7F25", "D=8B5A2B", "d=6B4220"},
};
ui::PixelArt const kIconTheNether{
    {
        "................",
        ".NNnNNNNnNNNNnN.",
        ".NnNNNnNNNNnNNN.",
        ".NNNLLNNNnNNNNN.",
        ".nNLlLNNNNNNnNN.",
        ".NNNLLNNnNNNNNN.",
        ".NnNNNNNNNNLLNN.",
        ".NNNNnNNNNLlLnN.",
        ".NNNNNNNnNNLLNN.",
        ".NnNNNNNNNNNNNN.",
        ".NNNNLLNNNnNNNN.",
        ".NNNLlLLNNNNNnN.",
        ".nNNNLLNNNNNNNN.",
        ".NNNNNNNNnNNNNN.",
        ".NNnNNNNNNNNnNN.",
        "................",
    },
    {"N=7A2A2A", "n=5A1C1C", "L=FF8A1E", "l=FFD23A"},
};
ui::PixelArt const kIconTheEnd{
    {
        "................",
        ".EEEeEEEEEEeEEE.",
        ".EeEEEEEPEEEEeE.",
        ".EEEEPEEEEEEEEE.",
        ".EEEEEEEEeEEPEE.",
        ".EeEEEEEEEEEEEE.",
        ".EEEEEEePEEEEeE.",
        ".EEPEEEEEEEEEEE.",
        ".EEEEEeEEEEPEEE.",
        ".EeEEEEEEEEEEEE.",
        ".EEEEEPEEEeEEEE.",
        ".EEEeEEEEEEEEPE.",
        ".EPEEEEEeEEEEEE.",
        ".EEEEEEEEEEPEEE.",
        ".EEeEEEPEEEEEeE.",
        "................",
    },
    {"E=DDDFA0", "e=B9BB7E", "P=6B3A8E"},
};
ui::PixelArt const kIconCommunityHubs{
    {
        "................",
        ".......KK.......",
        "......KbbK......",
        ".....KWBBbK.....",
        "....KWBBBBbK....",
        "....KWBBBBbK....",
        "....KBBBBBbK....",
        "....KBBBBBbK....",
        "...KBBBBBBBbK...",
        "...KBBBBBBBbK...",
        "..KBBBBBBBBBbK..",
        "..KbbbbbbbbbbK..",
        "...KKKKKKKKKK...",
        ".......KK.......",
        "......KbbK......",
        ".......KK.......",
    },
    {"K=503010", "B=FFC93A", "b=C08A17", "W=FFF0A8"},
};
ui::PixelArt const kIconDungeons{
    {
        "................",
        "....wwwwwwww....",
        "...wWWWWWWWWw...",
        "..wWWWWWWWWWWw..",
        "..wWWWWWWWWWWw..",
        "..wWKKKWWKKKWw..",
        "..wWKKKWWKKKWw..",
        "..wWKKKWWKKKWw..",
        "..wWWWWKKWWWWw..",
        "...wWWWKKWWWw...",
        "....wWWWWWWw....",
        "....wWKWKWKw....",
        "....wWWWWWWw....",
        ".....wwwwww.....",
        "................",
        "................",
    },
    {"W=E8E4D8", "w=9C968A", "K=202020"},
};
ui::PixelArt const kIconPlayerHomes{
    {
        "................",
        ".......rr.......",
        "......rRRr......",
        ".....rRRRRr.....",
        "....rRRRRRRr....",
        "...rRRRRRRRRr...",
        "..rRRRRRRRRRRr..",
        ".rrrrrrrrrrrrrr.",
        "..WWWWWWWWWWWW..",
        "..WGGWWWWWWGGW..",
        "..WGGWWDDWWGGW..",
        "..WWWWWDDWWWWW..",
        "..WWWWWDDWWWWW..",
        "..wwwwwDDwwwww..",
        "................",
        "................",
    },
    {"R=C2412E", "r=7F2B1E", "W=E8D4A2", "w=B89A68", "D=7A5230", "G=8FD3FF"},
};
ui::PixelArt const kIconResourceSites{
    {
        "................",
        "...iiiii........",
        "..iIIIIIii......",
        "...iiiiIIIi.....",
        "........iIIi....",
        ".......hHiIIi...",
        "......hHh.iIi...",
        ".....hHh...iIi..",
        "....hHh....iIi..",
        "...hHh......ii..",
        "..hHh...........",
        ".hHh............",
        ".hh.............",
        "................",
        "................",
        "................",
    },
    {"I=E0E0E0", "i=8C8C8C", "H=8A5E34", "h=5A3A1E"},
};
ui::PixelArt const kIconVillages{
    {
        "................",
        "......KKKK......",
        ".....KLLEEK.....",
        "....KLLEEEeK....",
        "...KLEEEEEEeK...",
        "..KLEEEEEEEEeK..",
        "..KEEEEEEEEEeK..",
        "..KEEEEEEEEEeK..",
        "..KEEEEEEEEEeK..",
        "..KEEEEEEEEeeK..",
        "...KEEEEEEeeK...",
        "....KEEEEeeK....",
        ".....KEeeeK.....",
        "......KKKK......",
        "................",
        "................",
    },
    {"K=0F5A2A", "E=3FD16A", "e=1F8F44", "L=B6F8C8"},
};
ui::PixelArt const kSpriteEdit{
    {
        "..........KK.",
        ".........KRRK",
        "........KYYRK",
        ".......KYYYK.",
        "......KYYYK..",
        ".....KYYYK...",
        "....KYYYK....",
        "...KYYYK.....",
        "..KWYYK......",
        "..KWWK.......",
        "..KKK........",
        ".............",
        ".............",
    },
    {"K=1E1E1E", "R=E88BA0", "Y=F2C230", "W=D8B482"},
};
ui::PixelArt const kSpriteDelete{
    {
        ".............",
        ".RR.......RR.",
        ".RRR.....RRR.",
        "..RRR...RRR..",
        "...RRR.RRR...",
        "....RRRRR....",
        ".....RRR.....",
        "....RRRRR....",
        "...RRR.RRR...",
        "..RRR...RRR..",
        ".RRR.....RRR.",
        ".RR.......RR.",
        ".............",
    },
    {"R=E03C3C"},
};
ui::PixelArt const kSpriteEye{
    {
        ".............",
        ".............",
        ".............",
        "....KKKKK....",
        "..KKWWWWWKK..",
        ".KWWWBBBWWWK.",
        "KWWWBBKBBWWWK",
        ".KWWWBBBWWWK.",
        "..KKWWWWWKK..",
        "....KKKKK....",
        ".............",
        ".............",
        ".............",
    },
    {"K=1E1E1E", "W=F2F2F2", "B=3B6FD6"},
};
ui::PixelArt const kSpriteBack{
    {
        "................",
        "................",
        "......K.........",
        ".....KK.........",
        "....KWK.........",
        "...KWWKKKKKKK...",
        "..KWWWWWWWWWWK..",
        ".KWWWWWWWWWWWK..",
        "..KWWWWWWWWWWK..",
        "...KWWKKKKKKK...",
        "....KWK.........",
        ".....KK.........",
        "......K.........",
        "................",
        "................",
        "................",
    },
    {"K=1E1E1E", "W=F2F2F2"},
};

struct NamedIcon {
    std::string_view    id;
    ui::PixelArt const* art;
};
NamedIcon const kIconTable[] = {
    {"favorites", &kIconFavorites},
    {"players", &kIconPlayers},
    {"global", &kIconGlobal},
    {"teams", &kIconTeams},
    {"dimension", &kIconDimension},
    {"overworld", &kIconOverworld},
    {"the_nether", &kIconTheNether},
    {"the_end", &kIconTheEnd},
    {"community_hubs", &kIconCommunityHubs},
    {"dungeons", &kIconDungeons},
    {"player_homes", &kIconPlayerHomes},
    {"resource_sites", &kIconResourceSites},
    {"villages", &kIconVillages},
};

} // namespace

void drawGroupIcon(ui::Painter& p, std::string_view icon, float x, float y, float scale, float alpha) {
    ui::PixelArt const* art = &kIconDimension;
    for (auto const& e : kIconTable) {
        if (e.id == icon) art = e.art;
    }
    p.pixels(*art, x, y, scale, ui::Painter::Layer::Foreground, alpha);
}

void drawEditSprite(ui::Painter& p, float x, float y, bool hovered) {
    if (hovered) p.fillRect({x - 1, y - 1, 15, 15}, ui::Color::argb(0x40FFFFFF), ui::Painter::Layer::Foreground);
    p.pixels(kSpriteEdit, x, y, 1, ui::Painter::Layer::Foreground);
}

void drawDeleteSprite(ui::Painter& p, float x, float y, bool active, bool hovered) {
    if (active && hovered) p.fillRect({x - 1, y - 1, 15, 15}, ui::Color::argb(0x40FFFFFF), ui::Painter::Layer::Foreground);
    p.pixels(kSpriteDelete, x, y, 1, ui::Painter::Layer::Foreground, active ? 1.f : 0.35f);
}

void drawEyeSprite(ui::Painter& p, float x, float y, bool hidden, bool hovered) {
    if (hovered) p.fillRect({x - 1, y - 1, 15, 15}, ui::Color::argb(0x40FFFFFF), ui::Painter::Layer::Foreground);
    p.pixels(kSpriteEye, x, y, 1, ui::Painter::Layer::Foreground, hidden ? 0.45f : 1.f);
    if (!hidden) return;
    // Struck through while hidden.
    for (int i = 0; i < 11; ++i) {
        p.fillRect({x + 1 + static_cast<float>(i), y + 11 - static_cast<float>(i), 1.5f, 1.5f}, ui::Color::rgb(0xE03C3C), ui::Painter::Layer::Foreground);
    }
}

void drawDragHandle(ui::Painter& p, float x, float y, bool hovered) {
    auto c = ui::Color::rgb(hovered ? 0xFFFFFF : 0xA0A0A0);
    for (int i = 0; i < 3; ++i) p.fillRect({x + 3, y + 6 + 3 * static_cast<float>(i), 10, 1}, c, ui::Painter::Layer::Foreground);
}

void drawBackSprite(ui::Painter& p, float x, float y) { p.pixels(kSpriteBack, x, y, 1, ui::Painter::Layer::Foreground); }

} // namespace bs::client::screens::groups

#endif // BS_CLIENT
