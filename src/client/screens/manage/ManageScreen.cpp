// Native Manage Waystones screen (docs/SPEC.md §4.2): the selection screen's frame (width 270,
// 64 px header, 22 px rows) with a drag handle, the name, the Personal Settings icon and either
// the Shift-gated delete X or the hide / show eye per row.

#ifdef BS_CLIENT

#include <algorithm>
#include <cctype>
#include <cmath>

#include "client/Client.h"
#include "client/input/InputRouter.h"
#include "client/screens/manage/Icons.h"
#include "client/ui/screen/Screens.h"
#include "client/ui/widgets/Style.h"
#include "client/ui/widgets/TextBox.h"
#include "client/ui/widgets/Widgets.h"
#include "mod/Log.h"

namespace bs::client::screens::manage {

namespace {

using namespace ui;

constexpr float  kWidth       = 270;
constexpr float  kHeader      = 64;
constexpr float  kRowH        = 22;
constexpr float  kRowW        = 220;
constexpr float  kAutoScroll  = 12;   ///< px from the list edge that scroll while dragging
constexpr double kDoubleClick = 0.3;  ///< seconds
constexpr int    VK_SHIFT_    = 0x10, VK_LSHIFT_ = 0xA0, VK_RSHIFT_ = 0xA1, VK_ESCAPE_ = 0x1B;

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

struct Row {
    std::string id, name, kind;
    bool        hidden = false, canDelete = false, canHide = false, everyone = false, current = false;
};

/// Hit areas of one row (relative to its rect).
struct RowParts {
    Rect handle, name, edit, action;
    explicit RowParts(Rect const& r)
    : handle{r.x, r.y + 1, 16, 20},
      name{r.x + 18, r.y + 1, kRowW - 18 - 32, 20},
      edit{r.x + kRowW - 30, r.y + 4, 13, 13},
      action{r.x + kRowW - 15, r.y + 4, 13, 13} {}
};

class ManageScreen final : public Screen {
public:
    ManageScreen() {
        mSearch.maxLength = 64;
        mSearch.onChange  = [this](std::string const&) { filter(); };
        mBack.icon        = [](Painter& p, Rect const& r, bool) { icons::centered(p, icons::kBack, r); };
        mBack.onPress     = [this] { action("back"); };
        mGroups.icon      = [](Painter& p, Rect const& r, bool) { icons::centered(p, icons::kGroups, r); };
        mGroups.onPress   = [this] { action("manage_groups"); };
        mList.rowHeight   = kRowH;
        mList.drawRow     = [this](Painter& p, int i, Rect const& r, bool hov) { drawRow(p, i, r, hov); };
        mList.clickRow    = [this](int i, MouseButton b, float x, float y) { return clickRow(i, b, x, y); };
    }

    void setModel(nlohmann::json const& model) override {
        mTitle     = model.value("title", ctr("gui.waystones.manage_waystones.title", "Manage Waystones"));
        mHasGroups = model.value("hasManageGroups", false);
        // The server answers every list action with one refresh. While answers are outstanding the
        // rows already show the optimistic result (e.g. several swaps of one drag): an older
        // intermediate order must not replace it.
        if (mPending > 0 && --mPending > 0) return;
        mRows.clear();
        for (auto const& r : model.value("rows", nlohmann::json::array())) {
            mRows.push_back({
                r.value("id", std::string{}),
                r.value("name", std::string{}),
                r.value("kind", std::string{}),
                r.value("hidden", false),
                r.value("canDelete", false),
                r.value("canHide", false),
                r.value("everyone", false),
                r.value("current", false),
            });
        }
        filter();
    }

    void render(Painter& p, FrameInfo const& f) override {
        drawBackground(p, f);
        double dt = mNow > 0 ? f.time - mNow : 0;
        mNow      = f.time;
        layout(f.screen);
        mMouseX = f.mouseX;
        mMouseY = f.mouseY;
        mTip.clear();
        if (!mDragId.empty()) autoScroll(static_cast<float>(dt));

        p.text(mTitle, mLeft, mTop + 20, Color::rgb(style::kWhite), true, 1, TextAlign::Center, kWidth);
        mSearch.render(p, f.time);
        mBack.render(p, f.mouseX, f.mouseY);
        mGroups.render(p, f.mouseX, f.mouseY);
        if (mBack.hovered(f.mouseX, f.mouseY)) mTip.set({{ctr("gui.waystones.manage_waystones.back", "Back")}}, f.mouseX, f.mouseY);
        if (mGroups.hovered(f.mouseX, f.mouseY)) {
            mTip.set({{ctr("gui.waystones.manage_waystones.manage_groups", "Manage Groups")}}, f.mouseX, f.mouseY);
        }

        mList.rowCount = static_cast<int>(mVisible.size());
        mList.render(p, f.mouseX, f.mouseY);
        if (mVisible.empty()) {
            auto msg = ctr("gui.waystones.waystone_selection.no_waystones_activated", "You have not activated any waystones yet.");
            p.text(msg, mLeft, mList.rect.y + 6, Color::rgb(style::kRed), true, 1, TextAlign::Center, kWidth);
        }
        if (!mDragId.empty()) mTip.clear();
        p.flush();
        mTip.draw(p);

        if (!mLogged) {
            mLogged = true;
            log().info(
                "manage screen: layout gui={}x{} left={} top={} rows={}",
                f.screen.w,
                f.screen.h,
                mLeft,
                mTop,
                mRows.size()
            );
        }
    }

    bool onMouseButton(MouseButton b, bool down, float x, float y) override {
        if (!down) {
            if (b == MouseButton::Left && !mDragId.empty()) mDragId.clear();
            mList.click(b, false, x, y);
            mSearch.onMouseButton(b == MouseButton::Right, false, x, y);
            return true;
        }
        if (mSearch.onMouseButton(b == MouseButton::Right, true, x, y)) return true;
        if (mBack.click(b, x, y) || (mGroups.visible && mGroups.click(b, x, y))) return true;
        mList.click(b, true, x, y);
        return true;
    }

    bool onMouseMove(float x, float y) override {
        mMouseX = x;
        mMouseY = y;
        if (mList.drag(x, y)) return true;
        if (!mDragId.empty()) dragTo(y);
        return true;
    }

    bool onScroll(float x, float y, float delta) override { return mList.wheel(x, y, delta); }

    bool onKey(int vk, bool down) override {
        if (vk == VK_SHIFT_ || vk == VK_LSHIFT_ || vk == VK_RSHIFT_) mShift = down;
        if (vk == VK_ESCAPE_) return false; // closes the screen
        if (mSearch.focused()) return mSearch.onKey(vk, down);
        return false;
    }
    bool onChar(std::string const& utf8) override { return mSearch.onChar(utf8); }
    [[nodiscard]] bool wantsTextInput() const override { return mSearch.focused(); }

private:
    [[nodiscard]] bool shift() const { return mShift || input::shiftDown(); }

    void layout(Rect const& screen) {
        float needed  = kHeader + kRowH * static_cast<float>(mVisible.size()) + 8;
        float height  = std::min(std::max(200.f, needed), screen.h - 40);
        mLeft         = std::floor((screen.w - kWidth) / 2);
        mTop          = std::floor((screen.h - height) / 2);
        float listX   = mLeft + (kWidth - kRowW) / 2;
        mSearch.rect  = {listX, mTop + 40, kRowW, 20};
        mBack.rect    = {mLeft - 8, mTop + 40, 20, 20};
        mGroups.rect  = {mLeft - 8, mTop + 64, 20, 20};
        mGroups.active = mHasGroups;
        mList.rect    = {listX, mTop + kHeader, kRowW + 8, std::max(kRowH, height - kHeader)};
        mList.clampScroll();
    }

    void filter() {
        auto needle = lower(mSearch.text());
        mVisible.clear();
        for (std::size_t i = 0; i < mRows.size(); ++i) {
            if (needle.empty() || lower(mRows[i].name).find(needle) != std::string::npos) mVisible.push_back(i);
        }
        if (!mDragId.empty() && visibleIndexOf(mDragId) < 0) mDragId.clear();
    }

    [[nodiscard]] int visibleIndexOf(std::string const& id) const {
        for (std::size_t i = 0; i < mVisible.size(); ++i) {
            if (mRows[mVisible[i]].id == id) return static_cast<int>(i);
        }
        return -1;
    }

    [[nodiscard]] Rect rowRect(int i) const { return {mList.rect.x, mList.rect.y + kRowH * static_cast<float>(i) - mList.scroll, kRowW, kRowH}; }

    void drawRow(Painter& p, int i, Rect const&, bool) {
        auto const& row  = mRows[mVisible[static_cast<std::size_t>(i)]];
        Rect        r    = rowRect(i);
        RowParts    parts(r);
        float       mx = mMouseX, my = mMouseY;
        bool        dragging = row.id == mDragId;

        if (dragging) style::outline(p, {r.x - 1, r.y, kRowW + 2, kRowH}, Color::rgb(style::kWhite));
        bool handleHover = parts.handle.contains(mx, my) || dragging;
        icons::centered(p, handleHover ? icons::kGripHover : icons::kGrip, parts.handle);
        if (parts.handle.contains(mx, my)) {
            std::vector<TooltipLine> lines;
            for (auto& l : icons::wrap(p, ctr("gui.waystones.manage_waystones.drag_to_reorder", "Drag to reorder. Double-click to move to top. Shift-double-click to move to bottom."))) {
                lines.push_back({std::move(l)});
            }
            mTip.set(std::move(lines), mx, my);
        }

        style::button(p, parts.name, false, false);
        auto name = row.name.empty() ? std::string{"<unnamed>"} : row.name;
        p.text(style::ellipsize(p, name, parts.name.w - 6), parts.name.x, parts.name.y + 6, Color::rgb(row.hidden ? 0x606060 : style::kButtonTextOff), true, 1, TextAlign::Center, parts.name.w);

        bool editHover = parts.edit.contains(mx, my);
        if (editHover) p.fillRect(parts.edit, Color::argb(0x40FFFFFF));
        icons::centered(p, icons::kPencil, parts.edit);
        if (editHover) mTip.set({{ctr("gui.waystones.manage_waystones.personal_settings", "Personal Settings")}}, mx, my);

        bool actionHover = parts.action.contains(mx, my);
        if (row.canDelete) {
            bool armed = shift();
            icons::centered(p, !armed ? icons::kDeleteOff : actionHover ? icons::kDeleteHover : icons::kDelete, parts.action);
            if (actionHover) {
                std::vector<TooltipLine> lines{
                    {armed ? ctr("gui.waystones.manage_waystones.delete", "Click to delete")
                           : ctr("gui.waystones.manage_waystones.delete_shift", "Hold shift and click to delete")}
                };
                if (row.everyone) {
                    lines.push_back({ctr("gui.waystones.manage_waystones.delete_warning", "This will remove the waystone for everyone."), style::kRed});
                }
                mTip.set(std::move(lines), mx, my);
            }
        } else if (row.canHide || row.hidden) {
            if (actionHover) p.fillRect(parts.action, Color::argb(0x40FFFFFF));
            icons::centered(p, row.hidden ? icons::kEyeClosed : icons::kEye, parts.action);
            if (actionHover) {
                mTip.set(
                    {{row.hidden ? ctr("gui.waystones.manage_waystones.show", "Show Waystone") : ctr("gui.waystones.manage_waystones.hide", "Hide Waystone")}},
                    mx,
                    my
                );
            }
        }
    }

    bool clickRow(int i, MouseButton b, float x, float y) {
        if (b != MouseButton::Left) return true;
        auto&    row = mRows[mVisible[static_cast<std::size_t>(i)]];
        RowParts parts(rowRect(i));
        if (parts.handle.contains(x, y)) {
            bool twice = row.id == mLastClickId && mNow - mLastClickAt < kDoubleClick;
            mLastClickId = row.id;
            mLastClickAt = mNow;
            if (twice) {
                mLastClickId.clear();
                moveToEnd(row.id, !shift());
                return true;
            }
            mDragId = row.id;
            return true;
        }
        if (parts.edit.contains(x, y)) {
            action("personal_settings", {{"id", row.id}});
            return true;
        }
        if (parts.action.contains(x, y)) {
            if (row.canDelete) {
                if (shift()) listAction("delete", {{"id", row.id}});
            } else if (row.canHide || row.hidden) {
                row.hidden = !row.hidden; // optimistic; the server answers with the real state
                listAction("set_hidden", {{"id", row.id}, {"hidden", row.hidden}});
            }
        }
        return true;
    }

    /// An action the server answers with a model refresh.
    void listAction(std::string const& name, nlohmann::json args) {
        ++mPending;
        action(name, std::move(args));
    }

    /// Double-click: to the top; shift-double-click: to the bottom.
    void moveToEnd(std::string const& id, bool top) {
        auto it = std::find_if(mRows.begin(), mRows.end(), [&](Row const& r) { return r.id == id; });
        if (it == mRows.end()) return;
        Row row = *it;
        mRows.erase(it);
        if (top) mRows.insert(mRows.begin(), row);
        else mRows.push_back(row);
        filter();
        listAction(top ? "move_top" : "move_bottom", {{"id", id}});
    }

    /// One swap per row passed, like Waystones' drag (each step sends a swap packet).
    void dragTo(float y) {
        int cur = visibleIndexOf(mDragId);
        if (cur < 0 || mVisible.empty()) return;
        float local  = std::clamp(y, mList.rect.y, mList.rect.bottom() - 1) - mList.rect.y + mList.scroll;
        int   target = std::clamp(static_cast<int>(std::floor(local / kRowH)), 0, static_cast<int>(mVisible.size()) - 1);
        while (cur != target) {
            int next = cur + (target > cur ? 1 : -1);
            auto& a  = mRows[mVisible[static_cast<std::size_t>(cur)]];
            auto& b  = mRows[mVisible[static_cast<std::size_t>(next)]];
            listAction("swap", {{"from", a.id}, {"to", b.id}});
            std::swap(a, b);
            cur = next;
        }
    }

    void autoScroll(float dt) {
        float speed = 120.f * std::clamp(dt, 0.f, 0.1f);
        if (mMouseY < mList.rect.y + kAutoScroll) mList.scroll -= speed;
        else if (mMouseY > mList.rect.bottom() - kAutoScroll) mList.scroll += speed;
        else return;
        mList.clampScroll();
        dragTo(mMouseY);
    }

    std::string           mTitle;
    bool                  mHasGroups = false;
    std::vector<Row>      mRows;
    std::vector<std::size_t> mVisible;
    TextBox               mSearch;
    Button                mBack, mGroups;
    ScrollList            mList;
    HoverTooltip          mTip;
    float                 mLeft = 0, mTop = 0, mMouseX = -1, mMouseY = -1;
    double                mNow = 0, mLastClickAt = -1;
    std::string           mLastClickId, mDragId;
    bool                  mShift = false, mLogged = false;
    int                   mPending = 0; ///< list actions not answered yet
};

} // namespace

} // namespace bs::client::screens::manage

BS_REGISTER_CLIENT_SCREEN("manage", [] { return std::make_unique<bs::client::screens::manage::ManageScreen>(); });

#endif // BS_CLIENT
