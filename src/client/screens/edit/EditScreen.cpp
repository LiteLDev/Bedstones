#ifdef BS_CLIENT

// Native Edit Waystone screen (docs/SPEC.md §4.5). Layout in Java GUI units on a 176 wide column
// centred on the screen (`top` = (height - 200) / 2):
//   title y 44 (centred), red error line y 56, name field (0, 70) 150x19, personal settings
//   button (155, 69) 21x21, visibility button (0, 95) 20x20 + label, manage modifiers button
//   (0, 120) 20x20 + label, Save / Close (38, 150) 100x20.

#include <cmath>
#include <format>
#include <string>
#include <vector>

#include "client/Client.h"
#include "client/screens/edit/ClickFilter.h"
#include "client/screens/edit/Icons.h"
#include "client/ui/screen/Screens.h"
#include "client/ui/widgets/Style.h"
#include "client/ui/widgets/TextBox.h"
#include "client/ui/widgets/Widgets.h"
#include "core/Waystone.h"
#include "mod/Log.h"

namespace bs::client::screens::edit {

namespace {

using namespace bs::client::ui;

constexpr int   kVkReturn = 0x0D;
constexpr int   kVkEscape = 0x1B;
constexpr float kWidth    = 176;
constexpr float kHeight   = 200;

std::string visibilityLabel(std::string const& v, std::string const& color) {
    if (v == "global") return ctr("gui.waystones.waystone_settings.visibility.global", "Visible to Everyone");
    if (v == "team") return ctr("gui.waystones.waystone_settings.visibility.team", "Visible to Team");
    if (v == "shard_only") return ctr("gui.waystones.waystone_settings.visibility.shard_only", "Teleports via Attuned Shard");
    if (v == "sharestone") {
        auto name = ctr("bs.edit.color." + color, color);
        return ctr("gui.waystones.waystone_settings.visibility.sharestone", "Visible to {} Sharestones", name);
    }
    return ctr("gui.waystones.waystone_settings.visibility.activation", "Visible upon Activation");
}

class EditScreen final : public Screen {
public:
    EditScreen() {
        mName.maxLength = 128;
        mName.placeholder = ctr("gui.waystones.waystone_settings.untitled", "Untitled Waystone");
    }

    void setModel(nlohmann::json const& m) override {
        mKind       = m.value("kind", std::string{"waystone"});
        mColor      = m.value("sharestoneColor", std::string{});
        mCanEdit    = m.value("canEdit", false);
        mOptions    = m.value("options", std::vector<std::string>{});
        mModCount   = m.value("modifierCount", 0);
        mModsShown  = m.value("modifiersAvailable", false);
        mPersonal   = m.value("personalSettings", false);
        mError.clear();
        if (auto const& e = m.value("error", nlohmann::json{}); e.is_object()) {
            mError = ctr(e.value("key", std::string{}), e.value("fallback", std::string{}));
        }
        if (!mInitialized) {
            // Later updates keep what the player typed / picked.
            mInitialized = true;
            mName.setText(m.value("name", std::string{}));
            auto current = m.value("visibility", std::string{});
            mVisIndex    = 0;
            for (std::size_t i = 0; i < mOptions.size(); ++i) {
                if (mOptions[i] == current) mVisIndex = i;
            }
            if (mCanEdit) mName.setFocused(true);
        }
        mName.textColor = mCanEdit ? 0xE0E0E0 : 0x707070;

        mPersonalBtn.onPress = [this] { send("personal_settings"); };
        mVisBtn.onPress      = [this] { cycle(1); };
        mVisBtn.onRightPress = [this] { cycle(-1); };
        mModsBtn.onPress     = [this] { send("modifiers"); };
        mSaveBtn.onPress     = [this] { send("save"); };
    }

    void render(Painter& p, FrameInfo const& f) override {
        drawBackground(p, f);
        float left = std::floor((f.screen.w - kWidth) / 2);
        float top  = std::floor((f.screen.h - kHeight) / 2);
        if (!mLoggedLayout) {
            mLoggedLayout = true;
            log().info("edit screen: layout gui={}x{} left={} top={}", f.screen.w, f.screen.h, left, top);
        }
        mTip.clear();
        float mx = f.mouseX, my = f.mouseY;

        auto typeName = mKind == "sharestone" ? ctr("bs.foundation.type.sharestone", "Sharestone")
                      : mKind == "warp_plate" ? ctr("bs.foundation.type.warp_plate", "Warp Plate")
                                              : ctr("bs.foundation.type.waystone", "Waystone");
        p.text(ctr("gui.waystones.waystone_settings.title", "Edit {}", typeName), left, top + 44, Color::rgb(style::kWhite), true, 1, TextAlign::Center, kWidth);
        if (!mError.empty()) p.text(mError, left - 40, top + 56, Color::rgb(style::kRed), true, 1, TextAlign::Center, kWidth + 80);

        // Name field + personal settings.
        mName.rect = {left, top + 70, 150, 19};
        mName.render(p, f.time);
        mPersonalBtn.rect    = {left + 155, top + 69, 21, 21};
        mPersonalBtn.visible = mPersonal;
        mPersonalBtn.icon    = [](Painter& pp, Rect const& r, bool) {
            pp.pixels(icons::kPersonal, r.x + std::floor((r.w - 14) / 2), r.y + std::floor((r.h - 12) / 2), 1, Painter::Layer::Foreground);
        };
        mPersonalBtn.render(p, mx, my);
        if (mPersonalBtn.hovered(mx, my)) {
            mTip.set({{ctr("gui.waystones.waystone_settings.personal_settings", "Personal Settings")}}, mx, my);
        }

        // Visibility.
        auto vis           = currentVisibility();
        auto visLabel      = visibilityLabel(vis, mColor);
        mVisBtn.rect       = {left, top + 95, 20, 20};
        mVisBtn.active     = mCanEdit && mOptions.size() > 1;
        mVisBtn.icon       = [this, vis](Painter& pp, Rect const& r, bool) { drawVisibilityIcon(pp, r, vis); };
        mVisBtn.render(p, mx, my);
        p.text(visLabel, left + 24, top + 101, Color::rgb(style::kWhite), true);
        if (mVisBtn.rect.contains(mx, my)) {
            std::vector<TooltipLine> lines{{visLabel}};
            if (!mCanEdit) lines.push_back({ctr("gui.waystones.waystone_settings.cannot_edit", "This waystone cannot be edited."), style::kRed});
            mTip.set(std::move(lines), mx, my);
        }

        // Modifiers.
        if (mModsShown) {
            mModsBtn.rect   = {left, top + 120, 20, 20};
            mModsBtn.active = mCanEdit;
            mModsBtn.icon   = [](Painter& pp, Rect const& r, bool) {
                pp.pixels(icons::kModifiers, r.x + 4, r.y + 5, 1, Painter::Layer::Foreground);
            };
            mModsBtn.render(p, mx, my);
            bool any   = mModCount > 0;
            auto label = any ? ctr("gui.waystones.waystone_settings.modifiers_active", "{} modifiers active", mModCount)
                             : ctr("gui.waystones.waystone_settings.no_modifiers_active", "No modifiers active");
            p.text(label, left + 24, top + 126, Color::rgb(any ? style::kGreen : style::kGray), true);
            if (mModsBtn.rect.contains(mx, my)) {
                mTip.set({{ctr("gui.waystones.waystone_settings.manage_modifiers", "Manage Modifiers")}}, mx, my);
            }
        }

        // Save / Close.
        mSaveBtn.rect  = {left + 38, top + 150, 100, 20};
        mSaveBtn.label = mCanEdit ? ctr("gui.waystones.waystone_settings.save", "Save") : ctr("gui.waystones.waystone_settings.close", "Close");
        mSaveBtn.render(p, mx, my);

        p.flush();
        mTip.draw(p);
    }

    bool onMouseButton(MouseButton b, bool down, float x, float y) override {
        if (!mClicks.accept(b, down, x, y)) return true;
        if (mCanEdit && b != MouseButton::Middle && mName.onMouseButton(b == MouseButton::Right, down, x, y)) return true;
        if (!down) return true;
        for (auto* btn : {&mPersonalBtn, &mVisBtn, &mModsBtn, &mSaveBtn}) {
            if (btn == &mModsBtn && !mModsShown) continue;
            if (btn->click(b, x, y)) return true;
        }
        return true;
    }

    bool onKey(int vk, bool down) override {
        if (down && (vk == kVkReturn || vk == kVkEscape)) {
            send("save");
            return true;
        }
        if (mCanEdit && mName.focused()) return mName.onKey(vk, down);
        return false;
    }

    bool onChar(std::string const& utf8) override { return mCanEdit && mName.onChar(utf8); }
    [[nodiscard]] bool wantsTextInput() const override { return mCanEdit && mName.focused(); }

private:
    [[nodiscard]] std::string currentVisibility() const {
        return mVisIndex < mOptions.size() ? mOptions[mVisIndex] : std::string{"activation"};
    }

    void cycle(int dir) {
        if (mOptions.empty()) return;
        auto n    = mOptions.size();
        mVisIndex = (mVisIndex + n + static_cast<std::size_t>(dir + static_cast<int>(n))) % n;
        log().info("edit screen: visibility -> {}", currentVisibility());
    }

    void send(std::string const& name) {
        if (mSent) return;
        mSent = true;
        mName.setFocused(false);
        action(name, {{"name", mName.text()}, {"visibility", currentVisibility()}});
    }

    void drawVisibilityIcon(Painter& p, Rect const& r, std::string const& vis) const {
        auto at = [&](PixelArt const& art) {
            p.pixels(art, r.x + std::floor((r.w - art.width()) / 2.f), r.y + std::floor((r.h - art.height()) / 2.f), 1, Painter::Layer::Foreground);
        };
        if (vis == "global") return at(icons::kGlobal);
        if (vis == "team") return at(icons::kTeam);
        if (vis == "shard_only") return at(icons::kShardOnly);
        if (vis == "sharestone") {
            std::uint32_t rgb = 0xFFFFFF;
            if (auto i = core::dyeIndex(mColor)) rgb = core::kDyeDiffuse[*i];
            mRune         = std::format("c={:06X}", rgb);
            PixelArt tint = icons::kSharestone;
            for (auto& e : tint.palette) {
                if (e.starts_with("c=")) e = mRune;
            }
            return at(tint);
        }
        at(icons::kActivation);
    }

    TextBox                  mName;
    Button                   mPersonalBtn, mVisBtn, mModsBtn, mSaveBtn;
    HoverTooltip             mTip;
    ClickFilter              mClicks;
    std::string              mKind, mColor, mError;
    std::vector<std::string> mOptions;
    std::size_t              mVisIndex    = 0;
    int                      mModCount    = 0;
    bool                     mCanEdit     = false;
    bool                     mModsShown   = false;
    bool                     mPersonal    = false;
    bool                     mInitialized = false;
    bool                     mLoggedLayout = false;
    bool                     mSent         = false;
    mutable std::string      mRune;
};

} // namespace

} // namespace bs::client::screens::edit

BS_REGISTER_CLIENT_SCREEN("edit", [] { return std::make_unique<bs::client::screens::edit::EditScreen>(); });

#endif // BS_CLIENT
