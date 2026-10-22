#ifdef BS_CLIENT

// Native waystone selection screen ("selection" and "admin_selection"), docs/SPEC.md §4.1.
//
// Layout (Java GUI coordinates, relative to the 270-wide panel at leftPos/topPos):
//   title           centred, y 0 (with a source waystone) or 20
//   current loc.    centred, y 20, "Current Location: " yellow + name white (hover: underline + pencil)
//   search box      x 25, y 40, 198x20 (220x20 without the sort button)
//   sort button     x 225, y 40, 20x20
//   left column     x -8: Manage Waystones 20x20 at y 40, group filters from y 65 every 22 px
//   list            x 25, y 64, rows 22 high and 220 wide, buttons 216x20

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mc/world/item/ItemStack.h"

#include "client/Client.h"
#include "client/screens/selection/Sprites.h"
#include "client/ui/screen/Screens.h"
#include "client/ui/widgets/Style.h"
#include "client/ui/widgets/TextBox.h"
#include "client/ui/widgets/Tooltip.h"
#include "client/ui/widgets/Widgets.h"
#include "core/selection/Selection.h"
#include "game/Items.h"
#include "mod/Log.h"

namespace bs::client::screens::selection {

namespace {

using namespace bs::client::ui;
namespace sel = core::selection;
using nlohmann::json;
using L = Painter::Layer;

struct ItemCostView {
    std::string id;
    int         count   = 0;
    bool        missing = false;
};

struct Entry {
    core::WarpTarget          target; ///< id, kind, name, pos.dim, groups, current
    double                    distance   = -1;
    bool                      selectable = false;
    std::string               color      = "white";
    int                       xp = 0, levels = 0, levelDisplay = 0;
    bool                      canAfford = true;
    double                    cooldown  = 0;
    std::vector<ItemCostView> items;
    std::string               refuse;
};

/// Draws the 1/2/3 marks of a level orb.
void levelMarks(Painter& p, float x, float y, int n, bool active) {
    Color mark   = Color::rgb(active ? 0xFFFFFF : 0xC6C6C6);
    Color shadow = Color::rgb(active ? 0x1E5E1E : 0x303030);
    float total  = static_cast<float>(n * 2 + (n - 1));
    float x0     = x + std::floor((16 - total) / 2);
    for (int i = 0; i < n; ++i) {
        float bx = x0 + static_cast<float>(i * 3);
        p.fillRect({bx + 1, y + 6, 2, 6}, shadow);
        p.fillRect({bx, y + 5, 2, 6}, mark);
    }
}

class SelectionScreen final : public Screen {
public:
    SelectionScreen() {
        mSearch.maxLength = 64;
        mSearch.onChange  = [this](std::string const& text) {
            mList.scroll = 0;
            auto n       = std::count_if(mEntries.begin(), mEntries.end(), [&](Entry const& e) { return sel::matchesSearch(displayName(e), text); });
            log().info("selection screen: search '{}' -> {} rows", text, n);
        };
    }

    void setModel(json const& m) override {
        mModel     = m;
        mAdmin     = m.value("admin", false);
        mHasSource = m.value("hasSource", false);
        mEntries.clear();
        mGroups.clear();
        for (auto const& g : m.value("groups", json::array())) {
            mGroups.push_back({g.value("id", std::string{}), g.value("name", std::string{}), g.value("icon", std::string{}),
                               g.value("color", std::string{}), g.value("hidden", false)});
        }
        std::string order;
        for (auto const& e : m.value("entries", json::array())) {
            Entry x;
            x.target.id      = e.value("id", std::string{});
            x.target.kind    = e.value("kind", std::string{});
            x.target.name    = e.value("name", std::string{});
            x.target.pos.dim = e.value("dim", 0);
            x.target.current = e.value("current", false);
            for (auto const& g : e.value("groups", json::array())) {
                if (g.is_string()) x.target.groups.push_back(g.get<std::string>());
            }
            x.distance   = e.value("distance", -1.0);
            x.selectable = e.value("selectable", false);
            x.color      = e.value("color", std::string{"white"});
            auto const& r = e.value("req", json::object());
            x.xp           = r.value("xp", 0);
            x.levels       = r.value("levels", 0);
            x.levelDisplay = r.value("levelDisplay", 0);
            x.canAfford    = r.value("canAfford", true);
            x.cooldown     = r.value("cooldown", 0.0);
            x.refuse       = r.value("refuse", std::string{});
            for (auto const& i : r.value("items", json::array())) {
                x.items.push_back({i.value("id", std::string{}), i.value("count", 0), i.value("missing", false)});
            }
            order += (order.empty() ? "" : "|") + x.target.name;
            mEntries.push_back(std::move(x));
        }
        mModelTime = -1;
        mLayoutLogged = false;
        if (!mFilter.empty() && std::none_of(mGroups.begin(), mGroups.end(), [&](auto const& g) { return g.id == mFilter; })) mFilter.clear();
        log().info("selection screen: rows={} sort={}", order, m.value("sortMode", std::string{"manual"}));
    }

    void render(Painter& p, FrameInfo const& f) override {
        drawBackground(p, f);
        mTooltip.clear();
        if (mModelTime < 0) mModelTime = f.time;
        mTime   = f.time;
        mMouseX = f.mouseX;
        mMouseY = f.mouseY;
        float mx = f.mouseX, my = f.mouseY;

        layout(f);
        auto visible = visibleEntries();

        // ---- header ----
        p.text(mModel.value("title", std::string{}), f.screen.x, mTop + (mHasSource ? 0 : 20), Color::rgb(style::kWhite), true, 1,
               TextAlign::Center, f.screen.w);
        if (mHasSource) drawCurrentLocation(p, f, mx, my);
        mSearch.render(p, f.time);
        if (mSort.visible) {
            mSort.render(p, mx, my);
            if (mSort.hovered(mx, my)) mTooltip.set({{sortTooltip(), style::kWhite}}, mx, my);
        }

        // ---- left column ----
        if (mManage.visible) {
            mManage.render(p, mx, my);
            if (mManage.hovered(mx, my)) {
                mTooltip.set({{ctr("gui.waystones.waystone_selection.manage_waystones", "Manage Waystones"), style::kWhite}}, mx, my);
            }
        }
        for (std::size_t i = 0; i < mGroupButtons.size(); ++i) {
            auto& b = mGroupButtons[i];
            b.render(p, mx, my);
            if (mGroupIds[i] == mFilter) style::outline(p, b.rect.inset(-1), Color::rgb(style::kWhite));
            if (b.hovered(mx, my)) mTooltip.set(b.tooltip, mx, my);
        }

        // ---- list ----
        mVisible = std::move(visible);
        mList.rowCount = static_cast<int>(mVisible.size());
        mList.drawRow  = [this](Painter& pp, int i, Rect const& row, bool) { drawRow(pp, *mVisible[static_cast<std::size_t>(i)], row); };
        mList.render(p, mx, my);
        if (auto empty = mModel.value("emptyText", std::string{}); !empty.empty()) {
            p.text(empty, f.screen.x, mList.rect.y + 6, Color::rgb(style::kRed), true, 1, TextAlign::Center, f.screen.w);
        }

        logLayout(f);
        mTooltip.draw(p);
    }

    bool onMouseButton(MouseButton b, bool down, float x, float y) override {
        if (mSearch.onMouseButton(b == MouseButton::Right, down, x, y)) return true;
        if (!down) return mList.click(b, down, x, y);
        if (mSort.click(b, x, y) || mManage.click(b, x, y)) return true;
        for (auto& g : mGroupButtons) {
            if (g.click(b, x, y)) return true;
        }
        if (b == MouseButton::Left && mHasSource && mCurrentHit.contains(x, y)) {
            action("edit_current");
            return true;
        }
        return mList.click(b, down, x, y);
    }
    bool onMouseMove(float x, float y) override { return mList.drag(x, y); }
    bool onScroll(float x, float y, float delta) override { return mList.wheel(x, y, delta); }
    bool onKey(int vk, bool down) override { return mSearch.focused() && mSearch.onKey(vk, down); }
    bool onChar(std::string const& utf8) override { return mSearch.focused() && mSearch.onChar(utf8); }
    [[nodiscard]] bool wantsTextInput() const override { return mSearch.focused(); }

private:
    // ---- layout ------------------------------------------------------------------------------------

    void layout(FrameInfo const& f) {
        int screenH = static_cast<int>(f.screen.h);
        mPanelH     = static_cast<float>(sel::panelHeight(static_cast<int>(mEntries.size()), screenH));
        mLeft       = f.screen.x + std::floor((f.screen.w - sel::kPanelWidth) / 2);
        mTop        = f.screen.y + std::floor((f.screen.h - mPanelH) / 2);

        bool showSort  = mModel.value("showSort", false);
        mSearch.rect   = {mLeft + 25, mTop + 40, showSort ? 198.f : 220.f, 20};
        mSort.visible  = showSort;
        mSort.rect     = {mLeft + 25 + 200, mTop + 40, 20, 20};
        mSort.onPress  = [this] { action("sort"); };
        mSort.icon     = [this](Painter& p, Rect const& r, bool) {
            auto mode = mModel.value("sortMode", std::string{"manual"});
            auto const& art = mode == "name" ? sprites::kSortName : mode == "distance" ? sprites::kSortDistance : sprites::kSortManual;
            p.pixels(art, r.x, r.y, 1, L::Foreground);
        };

        mManage.visible = mModel.value("showManage", false);
        mManage.active  = mModel.value("canManage", false);
        mManage.rect    = {mLeft - 8, mTop + 40, 20, 20};
        mManage.onPress = [this] { action("manage"); };
        mManage.icon    = [this](Painter& p, Rect const& r, bool) {
            p.pixels(sprites::kManage, r.x, r.y, 1, L::Foreground, mManage.active ? 1.f : 0.5f);
        };

        // Group filters: non-hidden groups with at least one entry, below the manage button.
        std::vector<core::WarpTarget> targets;
        for (auto const& e : mEntries) targets.push_back(e.target);
        auto groups = sel::filterGroups(mGroups, targets, sel::maxGroupButtons(static_cast<int>(mPanelH)));
        mGroupButtons.clear();
        mGroupIds.clear();
        float gy = mTop + 40 + 20 + 5;
        for (auto const& g : groups) {
            Button b;
            b.rect    = {mLeft - 8, gy, 20, 20};
            b.tooltip = {{g.name, style::formattingColor(g.color)}};
            b.icon    = [this, g](Painter& p, Rect const& r, bool) { drawGroupIcon(p, g, r.x, r.y); };
            b.onPress = [this, id = g.id] {
                mFilter      = mFilter == id ? std::string{} : id;
                mList.scroll = 0;
                log().info("selection screen: filter '{}'", mFilter);
            };
            mGroupButtons.push_back(std::move(b));
            mGroupIds.push_back(g.id);
            gy += 22;
        }
        if (!mFilter.empty() && std::find(mGroupIds.begin(), mGroupIds.end(), mFilter) == mGroupIds.end()) mFilter.clear();

        mList.rect             = {mLeft + 25, mTop + sel::kHeaderHeight, sel::kRowWidth + 8, mPanelH - sel::kHeaderHeight};
        mList.rowHeight        = sel::kRowHeight;
        mList.scrollbarWidth   = 6;
        mList.clickRow         = [this](int i, MouseButton b, float x, float y) {
            if (b != MouseButton::Left || i < 0 || i >= static_cast<int>(mVisible.size())) return true;
            auto const& e = *mVisible[static_cast<std::size_t>(i)];
            Rect        r = mList.rowRect(i);
            if (e.selectable && buttonRect(r).contains(x, y)) action("select", {{"id", e.target.id}});
            return true;
        };
    }

    [[nodiscard]] static Rect buttonRect(Rect const& row) { return {row.x + 2, row.y + 1, sel::kButtonWidth, sel::kButtonHeight}; }

    [[nodiscard]] std::vector<Entry const*> visibleEntries() const {
        std::vector<Entry const*> out;
        auto                      query = mSearch.text();
        for (auto const& e : mEntries) {
            if (!sel::matchesSearch(displayName(e), query)) continue;
            if (!mFilter.empty() && std::find(e.target.groups.begin(), e.target.groups.end(), mFilter) == e.target.groups.end()) continue;
            out.push_back(&e);
        }
        return out;
    }

    [[nodiscard]] static std::string displayName(Entry const& e) {
        return e.target.name.empty() ? ctr("bs.selection.unnamed", "<unnamed>") : e.target.name;
    }

    // ---- header ------------------------------------------------------------------------------------

    void drawCurrentLocation(Painter& p, FrameInfo const& f, float mx, float my) {
        auto  prefix = ctr("gui.waystones.waystone_selection.current_location", "Current Location: ");
        auto  name   = mModel.value("currentName", std::string{});
        if (name.empty()) name = ctr("bs.selection.unnamed", "<unnamed>");
        float wp = p.textWidth(prefix), wn = p.textWidth(name);
        float x0 = std::floor(f.screen.x + (f.screen.w - wp - wn) / 2);
        float y  = mTop + 20;
        mCurrentHit = {x0 + wp, y - 1, wn + 18, 11};
        bool hover  = mCurrentHit.contains(mx, my);
        p.text(prefix, x0, y, Color::rgb(style::kYellow), true);
        p.text(name, x0 + wp, y, Color::rgb(style::kWhite), true);
        if (hover) {
            p.fillRect({x0 + wp, y + 9, wn, 1}, Color::rgb(style::kWhite), L::Foreground);
            p.pixels(sprites::kEdit, x0 + wp + wn + 2, y - 4, 1, L::Foreground);
            mTooltip.set({{ctr("bs.selection.edit_current", "Edit Waystone"), style::kWhite}}, mx, my);
        }
    }

    [[nodiscard]] std::string sortTooltip() const {
        auto mode  = mModel.value("sortMode", std::string{"manual"});
        auto label = mode == "name"       ? ctr("gui.waystones.waystone_selection.sort.name", "By Name")
                   : mode == "distance" ? ctr("gui.waystones.waystone_selection.sort.distance", "By Distance")
                                        : ctr("gui.waystones.waystone_selection.sort.manual", "Manual");
        return ctr("gui.waystones.waystone_selection.sort", "Sort: {}", label);
    }

    void drawGroupIcon(Painter& p, sel::GroupInfo const& g, float x, float y) {
        if (g.icon.find(':') != std::string::npos) { // an item id
            p.item(stackFor(g.icon, 1), x, y);
            return;
        }
        if (g.icon == "favorites") return p.pixels(sprites::kStar, x, y, 1, L::Foreground);
        if (g.icon == "players") return p.pixels(sprites::kPlayer, x, y, 1, L::Foreground);
        if (g.icon == "teams") {
            p.pixels(sprites::kTeamMate, x - 3, y, 1, L::Foreground);
            return p.pixels(sprites::kPlayer, x + 3, y + 1, 1, L::Foreground);
        }
        if (g.icon == "global") return p.pixels(sprites::kGlobe, x, y, 1, L::Foreground);
        if (g.icon == "dimension" || g.id.starts_with("waystones:dimension/")) {
            auto dim = g.id.substr(g.id.rfind('/') + 1);
            return p.pixels(dim == "1" ? sprites::kNether : dim == "2" ? sprites::kEnd : sprites::kOverworld, x, y, 1, L::Foreground);
        }
        // User group: a tag in the group's colour.
        auto& art = tagArt(g.color);
        p.pixels(art, x, y, 1, L::Foreground);
    }

    PixelArt const& tagArt(std::string const& color) {
        auto it = mTags.find(color);
        if (it != mTags.end()) return it->second.art;
        auto& t = mTags[color];
        char  buf[16];
        std::snprintf(buf, sizeof(buf), "C=%06X", style::formattingColor(color) & 0xFFFFFF);
        t.palette = {buf, "k=1E1E1E", "w=1E1E1E"};
        t.art.rows.assign(std::begin(sprites::kTagRows), std::end(sprites::kTagRows));
        for (auto const& s : t.palette) t.art.palette.push_back(s);
        return t.art;
    }

    // ---- rows --------------------------------------------------------------------------------------

    ItemStack const& stackFor(std::string const& id, int count) {
        auto key = id + "#" + std::to_string(count);
        auto it  = mStacks.find(key);
        if (it == mStacks.end()) it = mStacks.emplace(key, std::make_unique<ItemStack>(game::makeItem(id, count))).first;
        return *it->second;
    }

    [[nodiscard]] double remainingCooldown(Entry const& e) const { return std::max(0.0, e.cooldown - (mTime - mModelTime)); }

    void drawRow(Painter& p, Entry const& e, Rect const& row) {
        Rect  b       = buttonRect(row);
        float mx      = mMouseX, my = mMouseY;
        bool  hovered = e.selectable && b.contains(mx, my);
        style::button(p, b, hovered, e.selectable);

        // Dimension overlay (Nether / End) on the right end of the button.
        if (e.target.pos.dim == 1 || e.target.pos.dim == 2) drawDimensionOverlay(p, {b.right() - 20, b.y, 20, 20}, e.target.pos.dim);

        // Left: requirement widgets.
        float x = b.x + 2;
        std::vector<TooltipLine> tip;
        if (!e.refuse.empty()) {
            p.pixels(sprites::kCancel, x, b.y + 2, 1, L::Foreground);
            x += 17;
            tip.push_back({e.refuse, style::kRed});
        }
        if (e.levelDisplay > 0) {
            auto ls     = sel::levelSprite(e.levelDisplay);
            bool active = e.canAfford;
            p.pixels(active ? sprites::kLevelOrb : sprites::kLevelOrbDisabled, x, b.y + 2, 1, L::Foreground);
            levelMarks(p, x, b.y + 2, ls.sprite, active);
            x += 17;
            if (ls.plus) {
                p.text("+", x, b.y + 6, Color::rgb(active ? style::kGreen : style::kGray), true);
                x += p.textWidth("+") + 2;
            }
        }
        if (e.xp > 0) tip.push_back({ctr("bs.selection.tooltip.xp_cost", "Experience Cost: {}", e.xp), style::kGreen});
        if (e.levels > 0) tip.push_back({ctr("bs.selection.tooltip.level_cost", "Level Cost: {}", e.levels), style::kGreen});
        if (double cd = remainingCooldown(e); cd > 0) {
            auto text = sel::formatCooldown(cd);
            p.text(text, x, b.y + 6, Color::rgb(style::kRed), true);
            x += p.textWidth(text) + 3;
            tip.push_back({ctr("bs.selection.tooltip.cooldown", "Cooldown: {} seconds", static_cast<long long>(std::ceil(cd))), style::kGold});
        }
        for (auto const& i : e.items) {
            auto const& stack = stackFor(i.id, std::max(1, i.count));
            p.item(stack, x, b.y + 2, 1, i.missing ? 0.5f : 1.f);
            if (i.count > 1) {
                auto c = std::to_string(i.count);
                p.text(c, x + 17 - p.textWidth(c), b.y + 11, Color::rgb(style::kWhite), true);
            }
            std::string itemName = i.id;
            if (auto lines = itemTooltipLines(stack); !lines.empty()) itemName = lines.front().text;
            tip.push_back({ctr("bs.selection.tooltip.item_cost", "Item Cost: {} {}", i.count, itemName), style::kWhite});
            x += 18;
        }
        float leftW = x - b.x;
        Rect  reqArea{b.x, b.y, leftW, b.h};
        if (!tip.empty() && reqArea.contains(mx, my)) mTooltip.set(tip, mx, my);

        // Right: distance (same dimension only).
        auto  name     = displayName(e);
        bool  longName = p.textWidth(name) > 120;
        float rightW   = 0;
        if (e.distance >= 0) {
            auto d = sel::formatDistance(e.distance, longName);
            rightW = p.textWidth(d) + 5;
            p.text(d, b.right() - rightW, b.y + 6, Color::rgb(e.selectable ? style::kGray : style::kDarkGray), true);
        }

        // Label: centred, coloured by group (warp portals light purple).
        float         side  = std::max(leftW, rightW);
        auto          shown = style::ellipsize(p, name, b.w - 2 * side - 4);
        std::uint32_t rgb   = e.selectable ? style::formattingColor(e.color) : style::kButtonTextOff;
        p.text(shown, b.x, b.y + 6, Color::rgb(rgb), true, 1, TextAlign::Center, b.w);
    }

    static void drawDimensionOverlay(Painter& p, Rect const& r, int dim) {
        std::uint32_t dark  = dim == 1 ? 0x6A1414 : 0xCFC98A;
        std::uint32_t light = dim == 1 ? 0xB0302A : 0xEDE7AE;
        for (int i = 0; i < 10; ++i) {
            for (int j = 0; j < 10; ++j) {
                bool  spot = (i * 7 + j * 13 + i * j) % 5 == 0;
                float a    = 0.35f + 0.05f * static_cast<float>(i); // fades in towards the right edge
                p.fillRect({r.x + 2.f * static_cast<float>(i), r.y + 2.f * static_cast<float>(j), 2, 2}, Color::rgb(spot ? light : dark, a));
            }
        }
    }

    // ---- e2e hook -----------------------------------------------------------------------------------

    void logLayout(FrameInfo const& f) {
        if (mLayoutLogged) return;
        mLayoutLogged = true;
        std::string groups;
        for (std::size_t i = 0; i < mGroupButtons.size(); ++i) {
            auto const& r = mGroupButtons[i].rect;
            groups += std::format("{}{}@{},{}", groups.empty() ? "" : ";", mGroupIds[i], r.x, r.y);
        }
        log().info(
            "selection screen: layout screen={}x{} panel={},{},{},{} title_y={} search={},{},{},{} sort={} manage={} list={},{} groups={}",
            f.screen.w, f.screen.h, mLeft, mTop, sel::kPanelWidth, mPanelH, mTop + (mHasSource ? 0 : 20), mSearch.rect.x, mSearch.rect.y,
            mSearch.rect.w, mSearch.rect.h, mSort.visible ? std::format("{},{}", mSort.rect.x, mSort.rect.y) : "none",
            mManage.visible ? std::format("{},{},{}", mManage.rect.x, mManage.rect.y, mManage.active ? "active" : "inactive") : "none",
            mList.rect.x, mList.rect.y, groups.empty() ? "none" : groups
        );
    }

    struct Tag {
        std::vector<std::string> palette;
        PixelArt                 art;
    };

    json                                              mModel = json::object();
    bool                                              mAdmin = false, mHasSource = false;
    std::vector<Entry>                                mEntries;
    std::vector<Entry const*>                         mVisible;
    std::vector<sel::GroupInfo>                       mGroups;
    std::string                                       mFilter;
    TextBox                                           mSearch;
    Button                                            mSort, mManage;
    std::vector<Button>                               mGroupButtons;
    std::vector<std::string>                          mGroupIds;
    ScrollList                                        mList;
    HoverTooltip                                      mTooltip;
    Rect                                              mCurrentHit;
    float                                             mLeft = 0, mTop = 0, mPanelH = 0;
    float                                             mMouseX = -1, mMouseY = -1;
    double                                            mModelTime = -1, mTime = 0;
    bool                                              mLayoutLogged = false;
    std::map<std::string, std::unique_ptr<ItemStack>> mStacks;
    std::map<std::string, Tag>                        mTags;

};

} // namespace

} // namespace bs::client::screens::selection

BS_REGISTER_CLIENT_SCREEN("selection", [] { return std::make_unique<bs::client::screens::selection::SelectionScreen>(); });
BS_REGISTER_CLIENT_SCREEN("admin_selection", [] { return std::make_unique<bs::client::screens::selection::SelectionScreen>(); });

#endif // BS_CLIENT
