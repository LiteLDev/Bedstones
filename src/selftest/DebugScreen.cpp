// `/waystones debug screen`: opens a small native test screen (client mod) or its form
// fallback. The e2e smoke scenarios use it to check the whole screen pipeline: server open ->
// client draw -> click -> server action -> close.

#include "ll/api/command/CommandHandle.h"
#include "ll/api/form/ModalForm.h"

#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"

#include "mod/Commands.h"
#include "mod/Log.h"
#include "net/Screens.h"

#ifdef BS_CLIENT
#include "client/ui/screen/Screens.h"
#include "client/ui/widgets/Style.h"
#include "client/ui/widgets/Widgets.h"
#endif

namespace bs::selftest {

namespace {
constexpr char const* kScreen = "foundation_test";
}

BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload().text("debug").text("screen").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* player = commands::playerOf(origin);
        if (!player) return output.error("Only players can open screens.");
        auto session = net::screens::open(*player, kScreen, {{"title", "Bedstones test screen"}, {"button", "Done"}});
        if (!session) return output.error("screen not shown");
        output.success("debug screen opened (session " + std::to_string(session) + ")");
    });
});

BS_REGISTER_SCREEN_ACTIONS(kScreen, [](Player& player, net::screens::ScreenSession&, std::string const& action, nlohmann::json const&) {
    log().info("debug screen: {} sent action {}", player.getRealName(), action);
    if (action == "done") net::screens::close(player);
});

BS_REGISTER_SCREEN_FALLBACK(kScreen, [](Player& player, net::screens::ScreenSession& session) {
    ll::form::ModalForm form(session.model.value("title", std::string{}), "", session.model.value("button", std::string{"Done"}), "Cancel");
    form.sendTo(player, [](Player& p, ll::form::ModalFormResult result, ll::form::FormCancelReason) {
        net::screens::simulateAction(p, result && *result == ll::form::ModalFormSelectedButton::Upper ? "done" : "close");
    });
});

} // namespace bs::selftest

#ifdef BS_CLIENT
namespace bs::client::ui {
namespace {

class DebugScreen final : public Screen {
public:
    void setModel(nlohmann::json const& model) override {
        mTitle         = model.value("title", std::string{});
        mButton.label  = model.value("button", std::string{"Done"});
        mButton.onPress = [this] { action("done"); };
    }
    void render(Painter& p, FrameInfo const& f) override {
        drawBackground(p, f);
        float cx = f.screen.w / 2;
        p.text(mTitle, 0, f.screen.h / 2 - 40, Color::rgb(style::kWhite), true, 1, TextAlign::Center, f.screen.w);
        mButton.rect = {cx - 50, f.screen.h / 2 - 10, 100, 20};
        mButton.render(p, f.mouseX, f.mouseY);
    }
    bool onMouseButton(MouseButton b, bool down, float x, float y) override { return down && mButton.click(b, x, y); }

private:
    std::string mTitle;
    Button      mButton;
};

} // namespace
} // namespace bs::client::ui

BS_REGISTER_CLIENT_SCREEN("foundation_test", [] { return std::make_unique<bs::client::ui::DebugScreen>(); });
#endif
