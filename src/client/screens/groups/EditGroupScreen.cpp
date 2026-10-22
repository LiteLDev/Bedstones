#ifdef BS_CLIENT

// Native Edit Group screen (docs/SPEC.md §4.4): 176x120, title "Edit Group", a name field drawn
// in the group colour (placeholder "Unnamed Group"), a 21x21 icon button and a 21x21 colour button
// (left click next, right click previous) and Save. Enter saves; Esc goes back to Manage Groups.

#include <array>
#include <cmath>
#include <string>

#include "client/Client.h"
#include "client/screens/groups/GroupIcons.h"
#include "client/ui/screen/Screens.h"
#include "client/ui/widgets/Style.h"
#include "client/ui/widgets/TextBox.h"
#include "client/ui/widgets/Widgets.h"
#include "core/groups/GroupRegistry.h"
#include "mod/Log.h"

namespace bs::client::screens::groups {

namespace {

using namespace bs::client::ui;
namespace cg = core::groups;

constexpr float kWidth  = 176;
constexpr float kHeight = 120;
constexpr int   kVkReturn = 0x0D, kVkEscape = 0x1B;

class EditGroupScreen final : public Screen {
public:
    EditGroupScreen() {
        mName.placeholder = ctr("gui.waystones.group_settings.name_placeholder", "Unnamed Group");
        mName.maxLength   = cg::kMaxNameLength;
        mIcon.icon        = [this](Painter& p, Rect const& r, bool) { drawGroupIcon(p, mIconId, r.x + 2.5f, r.y + 2.5f); };
        mIcon.onPress      = [this] { cycleIcon(1); };
        mIcon.onRightPress = [this] { cycleIcon(-1); };
        mColor.icon        = [this](Painter& p, Rect const& r, bool) {
            Rect sw{r.x + 5.5f, r.y + 5.5f, 10, 10};
            p.fillRect(sw.inset(-1), Color::rgb(0x000000), Painter::Layer::Foreground);
            p.fillRect(sw, Color::rgb(style::formattingColor(mColorId)), Painter::Layer::Foreground);
        };
        mColor.onPress      = [this] { cycleColor(1); };
        mColor.onRightPress = [this] { cycleColor(-1); };
        mSave.label         = ctr("gui.waystones.group_settings.save", "Save");
        mSave.onPress       = [this] { save(); };
    }

    void setModel(nlohmann::json const& model) override {
        if (mInit) return; // the fields are the player's edits from here on
        mInit    = true;
        mIconId  = model.value("icon", std::string(cg::kIcons.front()));
        mColorId = model.value("color", std::string{"white"});
        mName.setText(model.value("name", std::string{}));
        mName.setFocused(true);
    }

    void render(Painter& p, FrameInfo const& f) override {
        drawBackground(p, f);
        float left = std::floor((f.screen.w - kWidth) / 2);
        float top  = std::floor((f.screen.h - kHeight) / 2);
        mName.rect  = {left, top + 36, 128, 20};
        mIcon.rect  = {left + 132, top + 35.5f, 21, 21};
        mColor.rect = {left + 155, top + 35.5f, 21, 21};
        mSave.rect  = {left + (kWidth - 100) / 2, top + 96, 100, 20};
        mName.textColor = style::formattingColor(mColorId);

        p.text(ctr("gui.waystones.group_settings.title", "Edit Group"), left, top + 12, Color::rgb(style::kWhite), true, 1, TextAlign::Center, kWidth);
        mName.render(p, f.time);
        mIcon.render(p, f.mouseX, f.mouseY);
        mColor.render(p, f.mouseX, f.mouseY);
        mSave.render(p, f.mouseX, f.mouseY);
        p.flush();
        if (mIcon.hovered(f.mouseX, f.mouseY)) {
            drawTooltip(p, {{ctr("gui.waystones.group_settings.icon", "Icon: {}", iconName(mIconId))}}, f.mouseX, f.mouseY, nullptr, {}, false);
        } else if (mColor.hovered(f.mouseX, f.mouseY)) {
            drawTooltip(p, {{ctr("gui.waystones.group_settings.color", "Label Color: {}", colorName(mColorId))}}, f.mouseX, f.mouseY, nullptr, {}, false);
        }
        if (!mLogged) {
            mLogged = true;
            log().info("groups: edit_group layout screen={}x{} left={} top={}", f.screen.w, f.screen.h, left, top);
        }
    }

    bool onMouseButton(MouseButton b, bool down, float x, float y) override {
        if (!down) return true;
        if (mName.onMouseButton(b == MouseButton::Right, true, x, y)) return true;
        if (!mIcon.click(b, x, y) && !mColor.click(b, x, y)) mSave.click(b, x, y);
        return true;
    }

    bool onKey(int vk, bool down) override {
        if (vk == kVkReturn && down) {
            save();
            return true;
        }
        if (vk == kVkEscape && down) {
            action("back");
            return true;
        }
        return mName.onKey(vk, down);
    }
    bool onChar(std::string const& utf8) override { return mName.focused() && mName.onChar(utf8); }
    [[nodiscard]] bool wantsTextInput() const override { return mName.focused(); }

private:
    static std::string iconName(std::string const& id) { return ctr(cg::iconKey(id), cg::displayName(id)); }
    static std::string colorName(std::string const& id) { return ctr(cg::colorKey(id), cg::displayName(id)); }

    void cycleIcon(int dir) {
        mIconId = cg::cycleIcon(mIconId, dir);
        log().info("groups: edit_group icon -> {}", mIconId);
    }
    void cycleColor(int dir) {
        mColorId = cg::cycleColor(mColorId, dir);
        log().info("groups: edit_group color -> {}", mColorId);
    }
    void save() {
        action("save", {{"name", mName.text()}, {"icon", mIconId}, {"color", mColorId}});
    }

    TextBox     mName;
    Button      mIcon, mColor, mSave;
    std::string mIconId, mColorId;
    bool        mInit = false, mLogged = false;
};

} // namespace

} // namespace bs::client::screens::groups

BS_REGISTER_CLIENT_SCREEN("edit_group", [] { return std::make_unique<bs::client::screens::groups::EditGroupScreen>(); });

#endif // BS_CLIENT
