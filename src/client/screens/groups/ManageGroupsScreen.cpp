#ifdef BS_CLIENT

// Native Manage Groups screen (docs/SPEC.md §4.4): same frame as the selection / manage screens
// (width 270, 64 px header, 22 px rows), search box + "+" Create Group, Back, and one row per
// group: drag handle | icon + coloured name | edit | delete X (user groups, Shift) or hide toggle
// (inbuilt groups). Every change is an action the server re-validates; the server answers with
// an updated model.

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "client/Client.h"
#include "client/input/InputRouter.h"
#include "client/screens/groups/GroupIcons.h"
#include "client/ui/screen/Screens.h"
#include "client/ui/widgets/Style.h"
#include "client/ui/widgets/TextBox.h"
#include "client/ui/widgets/Widgets.h"
#include "mod/Log.h"

namespace bs::client::screens::groups {

namespace {

using namespace bs::client::ui;

constexpr float kWidth     = 270;
constexpr float kHeader    = 64;
constexpr float kRowHeight = 22;
constexpr float kRowWidth  = 220;
constexpr int   kVkShift = 0x10, kVkLShift = 0xA0, kVkRShift = 0xA1;
constexpr double kDoubleClick = 0.35; // seconds

struct GroupRow {
    std::string id, name, icon, color;
    bool        inbuilt = false, hidden = false;
};

std::string lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

class ManageGroupsScreen final : public Screen {
public:
    ManageGroupsScreen() {
        mSearch.placeholder = ctr("bs.groups.search", "Search...");
        mSearch.maxLength   = 64;
        mSearch.onChange    = [this](std::string const&) { refilter(); };
        mCreate.label       = "+";
        mCreate.tooltip     = {{ctr("gui.waystones.manage_groups.create_group", "Create Group")}};
        mCreate.onPress     = [this] {
            log().info("groups: client pressed create");
            action("create");
        };
        mBack.icon    = [](Painter& p, Rect const& r, bool) { drawBackSprite(p, r.x + 2, r.y + 2); };
        mBack.tooltip = {{ctr("gui.waystones.manage_groups.back", "Back")}};
        mBack.onPress = [this] { action("back"); };
        mList.rowHeight = kRowHeight;
        mList.drawRow   = [this](Painter& p, int i, Rect const& r, bool hov) { drawRow(p, i, r, hov); };
        mList.clickRow  = [this](int i, MouseButton b, float x, float y) { return clickRow(i, b, x, y); };
    }

    void setModel(nlohmann::json const& model) override {
        if (!mDragId.empty()) {
            // Keep the local order while dragging; the server's answer to the drop replaces it.
            mPendingModel = model;
            return;
        }
        mAll.clear();
        for (auto const& g : model.value("groups", nlohmann::json::array())) {
            mAll.push_back({
                g.value("id", std::string{}),
                g.value("name", std::string{}),
                g.value("icon", std::string{}),
                g.value("color", std::string{"white"}),
                g.value("inbuilt", false),
                g.value("hidden", false),
            });
        }
        refilter();
    }

    void render(Painter& p, FrameInfo const& f) override {
        drawBackground(p, f);
        mMouseX = f.mouseX;
        mMouseY = f.mouseY;
        mTime   = f.time;
        layout(f.screen);
        if (!mDragId.empty()) autoScroll();
        mTip.clear();

        p.text(ctr("gui.waystones.manage_groups.title", "Manage Groups"), mLeft, mTop + 20, Color::rgb(style::kWhite), true, 1, TextAlign::Center, kWidth);
        mSearch.render(p, f.time);
        mCreate.render(p, mMouseX, mMouseY);
        mBack.render(p, mMouseX, mMouseY);
        if (mCreate.hovered(mMouseX, mMouseY)) mTip.set(mCreate.tooltip, mMouseX, mMouseY);
        if (mBack.hovered(mMouseX, mMouseY)) mTip.set(mBack.tooltip, mMouseX, mMouseY);

        mList.rowCount = static_cast<int>(mVisible.size());
        mList.render(p, mMouseX, mMouseY);
        if (mVisible.empty()) {
            p.text(
                ctr("gui.waystones.manage_groups.no_groups", "There are no groups available."),
                mList.rect.x,
                mList.rect.y + 6,
                Color::rgb(style::kRed),
                true,
                1,
                TextAlign::Center,
                kRowWidth
            );
        }
        p.flush();
        mTip.draw(p);

        if (!mLoggedLayout) {
            mLoggedLayout = true;
            log().info(
                "groups: manage_groups layout screen={}x{} left={} top={} rows={}",
                f.screen.w,
                f.screen.h,
                mLeft,
                mTop,
                mVisible.size()
            );
        }
    }

    bool onMouseButton(MouseButton b, bool down, float x, float y) override {
        if (!down) {
            if (!mDragId.empty()) drop();
            mList.click(b, false, x, y);
            return true;
        }
        if (mSearch.onMouseButton(b == MouseButton::Right, true, x, y)) return true;
        if (mCreate.click(b, x, y) || mBack.click(b, x, y)) return true;
        mList.click(b, true, x, y);
        return true;
    }

    bool onMouseMove(float x, float y) override {
        mMouseX = x;
        mMouseY = y;
        mList.drag(x, y);
        if (!mDragId.empty()) dragTo(y);
        return true;
    }

    bool onScroll(float x, float y, float delta) override { return mList.wheel(x, y, delta); }

    bool onKey(int vk, bool down) override {
        if (vk == kVkShift || vk == kVkLShift || vk == kVkRShift) mShiftKey = down;
        if (mSearch.focused()) return mSearch.onKey(vk, down);
        return false;
    }
    bool onChar(std::string const& utf8) override { return mSearch.focused() && mSearch.onChar(utf8); }
    [[nodiscard]] bool wantsTextInput() const override { return mSearch.focused(); }

private:
    [[nodiscard]] bool shift() const { return mShiftKey || input::shiftDown(); }

    [[nodiscard]] int indexOf(std::string const& id) const {
        for (std::size_t i = 0; i < mAll.size(); ++i) {
            if (mAll[i].id == id) return static_cast<int>(i);
        }
        return -1;
    }

    void refilter() {
        mVisible.clear();
        auto q = lower(mSearch.text());
        for (std::size_t i = 0; i < mAll.size(); ++i) {
            if (q.empty() || lower(mAll[i].name).find(q) != std::string::npos) mVisible.push_back(static_cast<int>(i));
        }
        mList.rowCount = static_cast<int>(mVisible.size());
        mList.clampScroll();
    }

    void layout(Rect const& screen) {
        float needed = kHeader + kRowHeight * static_cast<float>(std::max<std::size_t>(mAll.size(), 1)) + 4;
        float height = std::min(std::max(200.f, needed), std::max(120.f, screen.h - 40));
        mLeft        = std::floor((screen.w - kWidth) / 2);
        mTop         = std::floor((screen.h - height) / 2);
        float listX  = mLeft + (kWidth - kRowWidth) / 2;
        mBack.rect   = {mLeft - 8, mTop + 40, 20, 20};
        mSearch.rect = {listX + 1, mTop + 40, 196, 20};
        mCreate.rect = {listX + kRowWidth - 20, mTop + 40, 20, 20};
        mList.rect   = {listX, mTop + kHeader, kRowWidth + mList.scrollbarWidth + 2, height - kHeader};
        // Rows snap to whole rows (the painter has no scissor).
        mList.rect.h = std::floor(mList.rect.h / kRowHeight) * kRowHeight;
    }

    // Row parts, relative to the row rect.
    static Rect handleRect(Rect const& r) { return {r.x + 2, r.y + 1, 16, 20}; }
    static Rect nameRect(Rect const& r) { return {r.x + 20, r.y + 1, 166, 20}; }
    static Rect editRect(Rect const& r) { return {r.x + 189, r.y + 4, 13, 13}; }
    static Rect lastRect(Rect const& r) { return {r.x + 205, r.y + 4, 13, 13}; }

    void drawRow(Painter& p, int i, Rect const& r, bool) {
        auto const& g        = mAll[static_cast<std::size_t>(mVisible[static_cast<std::size_t>(i)])];
        bool        dragging = g.id == mDragId;
        Rect        hr = handleRect(r), nr = nameRect(r), er = editRect(r), lr = lastRect(r);
        bool        overHandle = hr.contains(mMouseX, mMouseY) && mDragId.empty();
        drawDragHandle(p, hr.x, hr.y, overHandle || dragging);
        if (overHandle) {
            mTip.set({{ctr("gui.waystones.manage_groups.drag", "Drag to reorder. Double-click to move to top. Shift-double-click to move to bottom.")}}, mMouseX, mMouseY);
        }
        style::button(p, nr, false, true);
        if (dragging) style::outline(p, nr, Color::rgb(style::kWhite));
        drawGroupIcon(p, g.icon, nr.x + 3, nr.y + 2, 1, g.hidden ? 0.4f : 1.f);
        auto label = style::ellipsize(p, g.name, nr.w - 26);
        p.text(label, nr.x + 22, nr.y + 6, Color::rgb(g.hidden ? style::kGray : style::formattingColor(g.color)), true);

        bool overEdit = er.contains(mMouseX, mMouseY) && mDragId.empty();
        drawEditSprite(p, er.x, er.y, overEdit);
        if (overEdit) mTip.set({{ctr("gui.waystones.manage_groups.edit_group", "Edit Group")}}, mMouseX, mMouseY);

        bool overLast = lr.contains(mMouseX, mMouseY) && mDragId.empty();
        if (g.inbuilt) {
            drawEyeSprite(p, lr.x, lr.y, g.hidden, overLast);
            if (overLast) {
                mTip.set(
                    {{g.hidden ? ctr("gui.waystones.manage_groups.unhide_inbuilt_group", "Unhide Inbuilt Group")
                               : ctr("gui.waystones.manage_groups.hide_inbuilt_group", "Hide Inbuilt Group")}},
                    mMouseX,
                    mMouseY
                );
            }
        } else {
            bool active = shift();
            drawDeleteSprite(p, lr.x, lr.y, active, overLast);
            if (overLast) {
                mTip.set(
                    {{active ? ctr("gui.waystones.manage_groups.delete_click", "Click to delete")
                             : ctr("gui.waystones.manage_groups.delete_hold_shift", "Hold shift and click to delete")}},
                    mMouseX,
                    mMouseY
                );
            }
        }
    }

    bool clickRow(int i, MouseButton b, float x, float y) {
        if (b != MouseButton::Left) return true;
        auto const& g = mAll[static_cast<std::size_t>(mVisible[static_cast<std::size_t>(i)])];
        Rect        r = mList.rowRect(i);
        if (handleRect(r).contains(x, y)) {
            bool doubleClick = mLastHandleId == g.id && mTime - mLastHandleAt < kDoubleClick;
            mLastHandleId    = g.id;
            mLastHandleAt    = mTime;
            if (doubleClick) {
                int to = shift() ? static_cast<int>(mAll.size()) - 1 : 0;
                log().info("groups: client moved {} to {}", g.id, to);
                action("move", {{"id", g.id}, {"index", to}});
                return true;
            }
            if (!mSearch.text().empty()) return true; // reordering needs the full list
            mDragId    = g.id;
            mDragStart = indexOf(g.id);
            log().info("groups: client started dragging {}", g.id);
            return true;
        }
        if (editRect(r).contains(x, y)) {
            action("edit", {{"id", g.id}});
            return true;
        }
        if (lastRect(r).contains(x, y)) {
            if (g.inbuilt) {
                action("hide", {{"id", g.id}, {"hidden", !g.hidden}});
            } else if (shift()) {
                action("delete", {{"id", g.id}});
            }
        }
        return true;
    }

    /// Moves the dragged group to the row under `y` (locally; the server hears about it on drop).
    void dragTo(float y) {
        int cur = indexOf(mDragId);
        if (cur < 0 || mAll.size() < 2) return;
        int target = static_cast<int>(std::floor((y - mList.rect.y + mList.scroll) / kRowHeight));
        target     = std::clamp(target, 0, static_cast<int>(mAll.size()) - 1);
        while (cur != target) {
            int next = cur + (target > cur ? 1 : -1);
            std::swap(mAll[static_cast<std::size_t>(cur)], mAll[static_cast<std::size_t>(next)]);
            cur = next;
        }
        refilter();
    }

    /// Ends a drag: one "move" for the whole drag (the server re-validates and answers with the
    /// new order), then applies any model that arrived meanwhile.
    void drop() {
        auto id = std::exchange(mDragId, {});
        int  to = indexOf(id);
        log().info("groups: client dropped {} at {}", id, to);
        if (to >= 0 && to != mDragStart) action("move", {{"id", id}, {"index", to}});
        if (mPendingModel) {
            auto m = std::move(*mPendingModel);
            mPendingModel.reset();
            setModel(m);
        }
    }

    void autoScroll() {
        if (mMouseY < mList.rect.y + 12) mList.scroll -= 2;
        else if (mMouseY > mList.rect.bottom() - 12) mList.scroll += 2;
        else return;
        mList.clampScroll();
        dragTo(mMouseY);
    }

    std::vector<GroupRow> mAll;
    std::vector<int>      mVisible;
    TextBox               mSearch;
    Button                mCreate, mBack;
    ScrollList            mList;
    HoverTooltip          mTip;
    std::string           mDragId, mLastHandleId;
    int                   mDragStart = -1;
    std::optional<nlohmann::json> mPendingModel;
    double                mLastHandleAt = -1, mTime = 0;
    float                 mLeft = 0, mTop = 0, mMouseX = -1, mMouseY = -1;
    bool                  mShiftKey = false, mLoggedLayout = false;
};

} // namespace

} // namespace bs::client::screens::groups

BS_REGISTER_CLIENT_SCREEN("manage_groups", [] { return std::make_unique<bs::client::screens::groups::ManageGroupsScreen>(); });

#endif // BS_CLIENT
