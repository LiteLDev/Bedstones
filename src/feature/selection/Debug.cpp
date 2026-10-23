// Debug helpers of the selection screen (e2e scenarios, manual testing):
//   /waystones debug selection          open the selection menu (source: the setup's home waystone)
//   /waystones debug selection setup    create activated test waystones around the player
//   /waystones debug selection costs    toggle the requirement preview (see Selection.h)
//   /waystones debug selection admin    open the admin selection of yourself
//   /waystones debug selection clear    remove the test waystones

#include <cmath>

#include "ll/api/command/CommandHandle.h"

#include "mc/deps/core/math/Vec2.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/server/commands/CommandOrigin.h"
#include "mc/server/commands/CommandOutput.h"
#include "mc/world/actor/player/Player.h"

#include "feature/selection/Selection.h"
#include "game/Blocks.h"
#include "game/Waystones.h"
#include "mod/Commands.h"
#include "mod/Log.h"
#include "service/Services.h"

namespace bs::feature::selection {

namespace {

constexpr char const* kPreviewKey = "selectionPreview";
constexpr char const* kDebugKey   = "selectionDebug";

// ---- requirement preview ----------------------------------------------------------------------------

class PreviewRequirements final : public service::IRequirements {
public:
    explicit PreviewRequirements(std::unique_ptr<service::IRequirements> inner) : mInner(std::move(inner)) {}

    core::Requirement resolve(Player& player, core::WarpSource const& source, core::WarpTarget const& target) override {
        auto r = mInner->resolve(player, source, target);
        auto* rec = game::Waystones::get().find(target.id);
        if (!rec) return r;
        auto it = rec->extra.find(kPreviewKey);
        if (it == rec->extra.end()) return r;
        auto add = core::Requirement::fromJson(*it);
        r.xpPoints += add.xpPoints;
        r.levels += add.levels;
        r.activeCooldown = std::max(r.activeCooldown, add.activeCooldown);
        r.items.insert(r.items.end(), add.items.begin(), add.items.end());
        if (!add.refuse.empty()) r.refuse = add.refuse;
        return r;
    }
    bool canAfford(Player& player, core::Requirement const& r) override {
        if (player.isCreative()) return true;
        if (!r.refuse.empty() || r.activeCooldown > 0) return false;
        int    level = playerLevel(player);
        double have  = core::totalXpForLevel(level) + playerProgress(player) * core::xpForLevel(level);
        if (r.levels > level || r.xpPoints > have) return false;
        for (auto const& i : r.items) {
            if (countItem(player, i.id) < i.count) return false;
        }
        return mInner->canAfford(player, r);
    }
    void consume(Player& player, core::Requirement const& r) override { mInner->consume(player, r); }

    std::unique_ptr<service::IRequirements> release() { return std::move(mInner); }

private:
    std::unique_ptr<service::IRequirements> mInner;
};

PreviewRequirements* gPreview = nullptr;

// ---- test waystones -----------------------------------------------------------------------------------

struct Spec {
    char const* name;
    int         dim;
    int         dx, dz; ///< offset from the player (overworld) or absolute x/z (other dimensions)
    bool        global;
    bool        activate;
    char const* preview; ///< requirement json
    bool        favorite;
};

constexpr Spec kSpecs[]{
    {"Spawn Hearth", 0, 2, 0, false, true, nullptr, false},
    {"Alpha Village", 0, 50, 0, false, true, R"({"xpPoints": 3})", true},
    {"brook crossing", 0, 0, 320, false, true, R"({"levels": 2})", false},
    {"Market Square", 0, 150, 0, true, false, nullptr, false},
    {"Forbidden Keep", 0, 0, 600, false, true, R"({"refuse": "This waystone is sealed."})", false},
    {"Cooldown Camp", 0, -800, 0, false, true, R"({"activeCooldown": 125})", false},
    {"Iron Mine", 0, 0, 1200, false, true, R"({"items": [{"id": "minecraft:ender_pearl", "count": 3}]})", false},
    {"Far Lighthouse", 0, 15000, 0, false, true, R"({"levels": 30})", false},
    {"Nether Hub", 1, 10, 10, false, true, R"({"xpPoints": 27})", false},
    {"End Gate", 2, 100, 0, false, true, R"({"levels": 3})", false},
};

void clearDebugWaystones() {
    auto&                    ws = game::Waystones::get();
    std::vector<std::string> uids;
    for (auto* r : ws.all([](core::WaystoneRecord const& r) { return r.extra.contains(kDebugKey); })) uids.push_back(r->uid);
    for (auto const& uid : uids) ws.remove(uid);
}

std::string setupDebugWaystones(Player& player) {
    clearDebugWaystones();
    auto& ws   = game::Waystones::get();
    auto  pos  = player.getPosition();
    int   px   = static_cast<int>(std::floor(pos.x)), py = static_cast<int>(std::floor(pos.y)), pz = static_cast<int>(std::floor(pos.z));
    auto& data = ws.playerData(player);
    std::string home;
    for (auto const& s : kSpecs) {
        core::WaystoneRecord r;
        r.name       = s.name;
        r.style      = "waystone";
        r.origin     = core::Origin::Player;
        r.visibility = s.global ? core::Visibility::Global : core::Visibility::Activation;
        r.pos        = s.dim == 0 ? core::GlobalPos{0, px + s.dx, py, pz + s.dz} : core::GlobalPos{s.dim, s.dx, 64, s.dz};
        r.extra[kDebugKey] = true;
        if (s.preview) r.extra[kPreviewKey] = nlohmann::json::parse(s.preview);
        auto* created = ws.create(std::move(r));
        if (!created) continue;
        game::placeRecordBlock(*created);
        if (s.activate) ws.activate(player, created->uid);
        if (s.favorite) data.groups[created->uid] = {std::string(service::groups::kFavorites)};
        if (home.empty()) home = created->uid;
    }
    ws.markPlayerDirty(game::Waystones::playerKey(player));
    return home;
}

core::WarpSource debugSource() {
    core::WarpSource src;
    src.kind = core::WarpSourceKind::Command;
    for (auto* r : game::Waystones::get().all([](core::WaystoneRecord const& r) { return r.extra.contains(kDebugKey); })) {
        src.kind    = core::WarpSourceKind::Waystone;
        src.fromUid = r->uid;
        src.fromPos = r->pos;
        break;
    }
    return src;
}

} // namespace

void setRequirementPreview(bool on) {
    auto& slot = service::detail::slot<service::IRequirements>();
    if (on) {
        if (gPreview && slot.get() == gPreview) return;
        (void)service::get<service::IRequirements>(); // materialize the current implementation
        auto wrapper = std::make_unique<PreviewRequirements>(std::move(slot));
        gPreview     = wrapper.get();
        slot         = std::move(wrapper);
        return;
    }
    if (!gPreview) return;
    if (slot.get() == gPreview) slot = gPreview->release(); // restore the wrapped implementation
    gPreview = nullptr;
}

bool requirementPreview() { return gPreview && service::detail::slot<service::IRequirements>().get() == gPreview; }

BS_REGISTER_SUBCOMMAND([](ll::command::CommandHandle& cmd) {
    cmd.overload().text("debug").text("selection").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* p = commands::playerOf(origin);
        if (!p) return output.error("Only players can open screens.");
        if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
        auto src = debugSource();
        // A menu opened from a waystone closes when the player is out of its reach (teleport unit):
        // stand next to the home waystone first, as a real player would.
        if (src.kind == core::WarpSourceKind::Waystone && static_cast<int>(p->getDimensionId()) == src.fromPos.dim) {
            Vec3 at{static_cast<float>(src.fromPos.x) + 0.5f, static_cast<float>(src.fromPos.y), static_cast<float>(src.fromPos.z) - 1.5f};
            if (p->getPosition().distanceTo(at) > 3.f) p->teleport(at, p->getDimensionId(), Vec2{0.f, 0.f});
        }
        service::get<service::ISelection>().open(*p, src);
        output.success("selection opened");
    });
    cmd.overload().text("debug").text("selection").text("setup").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* p = commands::playerOf(origin);
        if (!p) return output.error("Only players can run this.");
        if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
        auto home = setupDebugWaystones(*p);
        log().info("selection debug: created {} waystones for {} (home {})", std::size(kSpecs), p->getRealName(), home);
        output.success("selection debug waystones created");
    });
    cmd.overload().text("debug").text("selection").text("costs").execute([](CommandOrigin const& origin, CommandOutput& output) {
        if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
        setRequirementPreview(!requirementPreview());
        log().info("selection debug: requirement preview {}", requirementPreview() ? "on" : "off");
        output.success(std::string("requirement preview ") + (requirementPreview() ? "on" : "off"));
    });
    cmd.overload().text("debug").text("selection").text("admin").execute([](CommandOrigin const& origin, CommandOutput& output) {
        auto* p = commands::playerOf(origin);
        if (!p) return output.error("Only players can open screens.");
        if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
        service::get<service::ISelection>().openAdmin(*p, game::Waystones::playerKey(*p));
        output.success("admin selection opened");
    });
    cmd.overload().text("debug").text("selection").text("clear").execute([](CommandOrigin const& origin, CommandOutput& output) {
        if (!commands::isAdmin(origin)) return output.error("You need operator permission.");
        clearDebugWaystones();
        log().info("selection debug: cleared");
        output.success("selection debug waystones removed");
    });
});

} // namespace bs::feature::selection
