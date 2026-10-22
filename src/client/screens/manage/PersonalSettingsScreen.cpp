// Native Personal Settings screen (docs/SPEC.md §4.3): width 176, the waystone name as title at
// y 44, alias field, Edit Waystone pencil, group cycle button, Manage Groups, Favorite, Save.
// Enter or Esc saves and returns to Manage Waystones.

#ifdef BS_CLIENT

#include <cmath>

#include "client/Client.h"
#include "client/screens/manage/Icons.h"
#include "client/ui/screen/Screens.h"
#include "client/ui/widgets/Style.h"
#include "client/ui/widgets/TextBox.h"
#include "client/ui/widgets/Widgets.h"
#include "core/manage/ManageRules.h"
#include "mod/Log.h"

namespace bs::client::screens::manage {

namespace {

using namespace ui;

constexpr float kWidth  = 176;
constexpr float kHeight = 200;
constexpr int   VK_RETURN_ = 0x0D, VK_ESCAPE_ = 0x1B;

struct Group {
    std::string   id, name;
    std::uint32_t rgb = style::kWhite;
};

class PersonalSettingsScreen final : public Screen {
public:
    PersonalSettingsScreen() {
        mAlias.maxLength   = 128;
        mAlias.placeholder = ctr("gui.waystones.personal_waystone_settings.no_alias", "No alias");
        mEdit.icon         = [](Painter& p, Rect const& r, bool) { icons::centered(p, icons::kPencil, r); };
        mEdit.tooltip      = {{ctr("gui.waystones.personal_waystone_settings.edit_waystone", "Edit Waystone")}};
        mEdit.onPress      = [this] { send("edit"); };
        mGroup.onPress     = [this] { cycle(1); };
        mGroup.onRightPress = [this] { cycle(-1); };
        mManageGroups.icon = [](Painter& p, Rect const& r, bool) { icons::centered(p, icons::kGroups, r); };
        mManageGroups.tooltip = {{ctr("gui.waystones.manage_waystones.manage_groups", "Manage Groups")}};
        mManageGroups.onPress = [this] { send("manage_groups"); };
        mFavorite.label    = ctr("gui.waystones.personal_waystone_settings.favorite", "Favorite");
        mSave.label        = ctr("gui.waystones.personal_waystone_settings.save", "Save");
        mSave.onPress      = [this] { send("save"); };
    }

    void setModel(nlohmann::json const& m) override {
        mName = m.value("name", std::string{});
        mAlias.setText(m.value("alias", std::string{}));
        mCanEdit           = m.value("canEdit", false);
        mManageGroups.active = m.value("hasManageGroups", false);
        mGroups.clear();
        for (auto const& g : m.value("groups", nlohmann::json::array())) {
            mGroups.push_back({g.value("id", std::string{}), g.value("name", std::string{}), style::formattingColor(g.value("color", std::string{"white"}))});
        }
        mGroupId          = m.value("group", std::string{});
        mFavorite.checked = m.value("favorite", false);
        mAlias.setFocused(true);
        refreshGroupButton();
    }

    void render(Painter& p, FrameInfo const& f) override {
        drawBackground(p, f);
        float left = std::floor((f.screen.w - kWidth) / 2), top = std::floor((f.screen.h - kHeight) / 2);
        mAlias.rect        = {left, top + 60, mCanEdit ? 150.f : kWidth, 19};
        mEdit.rect         = {left + 155, top + 59, 21, 21};
        mEdit.visible      = mCanEdit;
        mGroup.rect        = {left, top + 86, 150, 20};
        mManageGroups.rect = {left + 156, top + 86, 20, 20};
        mFavorite.rect     = {left, top + 112, 20, 20};
        mSave.rect         = {left + (kWidth - 100) / 2, top + 142, 100, 20};

        auto title = mName.empty() ? std::string{"<unnamed>"} : mName;
        p.text(title, left, top + 44, Color::rgb(style::kWhite), true, 1, TextAlign::Center, kWidth);
        mAlias.render(p, f.time);
        mEdit.render(p, f.mouseX, f.mouseY);
        mGroup.render(p, f.mouseX, f.mouseY);
        mManageGroups.render(p, f.mouseX, f.mouseY);
        mFavorite.render(p, f.mouseX, f.mouseY);
        mSave.render(p, f.mouseX, f.mouseY);

        mTip.clear();
        for (auto const* b : {&mEdit, &mManageGroups}) {
            if (b->hovered(f.mouseX, f.mouseY)) mTip.set(b->tooltip, f.mouseX, f.mouseY);
        }
        p.flush();
        mTip.draw(p);

        if (!mLogged) {
            mLogged = true;
            log().info("personal settings screen: layout gui={}x{} left={} top={} edit={}", f.screen.w, f.screen.h, left, top, mCanEdit);
        }
    }

    bool onMouseButton(MouseButton b, bool down, float x, float y) override {
        if (!mLatch.accept(b, down)) return true;
        if (mAlias.onMouseButton(b == MouseButton::Right, down, x, y)) return true;
        if (!down) return true;
        if (mEdit.click(b, x, y) || mGroup.click(b, x, y) || mManageGroups.click(b, x, y) || mSave.click(b, x, y)) return true;
        mFavorite.click(b, x, y);
        return true;
    }

    bool onKey(int vk, bool down) override {
        if (vk == VK_RETURN_ || vk == VK_ESCAPE_) {
            if (down) send("save");
            return true;
        }
        return mAlias.focused() && mAlias.onKey(vk, down);
    }
    bool onChar(std::string const& utf8) override { return mAlias.onChar(utf8); }
    [[nodiscard]] bool wantsTextInput() const override { return mAlias.focused(); }

private:
    void send(std::string const& name) {
        if (mSent) return; // one answer per screen: the server opens the next one
        mSent = true;
        action(name, {{"alias", mAlias.text()}, {"group", mGroupId}, {"favorite", mFavorite.checked}});
    }

    void cycle(int dir) {
        std::vector<std::string> ids;
        for (auto const& g : mGroups) ids.push_back(g.id);
        mGroupId = core::manage::cycleGroup(ids, mGroupId, dir);
        refreshGroupButton();
    }

    void refreshGroupButton() {
        mGroup.icon       = nullptr;
        mGroup.labelColor = style::kWhite;
        mGroup.active     = !mGroups.empty();
        if (mGroups.empty()) {
            mGroup.label = ctr("gui.waystones.personal_waystone_settings.no_groups_defined", "No groups defined");
            return;
        }
        for (auto const& g : mGroups) {
            if (g.id != mGroupId) continue;
            mGroup.label      = g.name;
            mGroup.labelColor = g.rgb;
            mGroup.icon       = [rgb = g.rgb](Painter& p, Rect const& r, bool) { icons::groupTag(p, r, rgb); };
            return;
        }
        mGroupId     = {};
        mGroup.label = ctr("gui.waystones.personal_waystone_settings.no_group", "No Group");
    }

    std::string        mName, mGroupId;
    TextBox            mAlias;
    Button             mEdit, mGroup, mManageGroups, mSave;
    Checkbox           mFavorite;
    HoverTooltip       mTip;
    std::vector<Group> mGroups;
    icons::ButtonLatch mLatch;
    bool               mCanEdit = false, mLogged = false, mSent = false;
};

} // namespace

} // namespace bs::client::screens::manage

BS_REGISTER_CLIENT_SCREEN("personal_settings", [] { return std::make_unique<bs::client::screens::manage::PersonalSettingsScreen>(); });

#endif // BS_CLIENT
