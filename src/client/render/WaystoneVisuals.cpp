// Client visuals of waystones, sharestones and portstones (U17, client build):
//   - rune glow: rune glyphs on the four sides of activated waystones (style tint), of every
//     sharestone and portstone (dye tint), full-bright unless `client.disableTextGlow`;
//   - ambient particles: 75% chance per tick, 1 portal + 1 enchant particle around activated
//     waystones within 16 blocks;
//   - a floating, spinning warp stone above sharestones.
//
// Bedrock has no block-entity renderers for custom blocks, so everything is drawn with
// client-only particle effects from our resource pack (`particles/bs_visuals_*.json`), spawned
// straight into the client level (client/render/ClientParticles.h): flat quads oriented by a
// Molang direction, tinted by Molang colour variables, without the lighting component (meant to be
// full-bright) or with it (`disableTextGlow`). Note: measured in 1.26.51, every particle (also
// vanilla ones such as the end rod) takes the world's light, so both variants currently look the
// same in the dark; real full-bright needs a renderer hook. They are respawned with overlapping
// lifetimes (alpha-tested, so the overlap is invisible) so they look continuous. Nothing touches block states, and only this player sees them, which
// makes the runes per-player (activation-dependent) like Waystones' block entity renderer.
//
// The data comes from the server's `visuals.waystones` sync (feature/visuals/VisualsSync.h).
// Portstones have no record: the client finds them by scanning the blocks around the player.

#ifdef BS_CLIENT

#include <cmath>
#include <random>
#include <string>
#include <unordered_set>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/ClientLevelTickEvent.h"

#include "mc/client/player/LocalPlayer.h"
#include "mc/deps/core/math/Vec3.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/Tick.h"
#include "mc/world/level/block/Block.h"

#include "client/Client.h"
#include "client/net/ClientNet.h"
#include "client/render/ClientParticles.h"
#include "core/visuals/Visuals.h"
#include "game/Blocks.h"
#include "game/Ids.h"
#include "mod/Config.h"
#include "mod/Log.h"
#include "mod/Module.h"

namespace bs::client::render {

/// Config section `client` (Waystones' client config).
struct ClientConfig {
    int  version         = 1;
    /// Draw the runes with world lighting instead of full-bright.
    bool disableTextGlow = false;
};

namespace {

namespace cv = core::visuals;

constexpr char const* kRuneGlow     = "bedstones:visuals_rune";
constexpr char const* kRuneLit      = "bedstones:visuals_rune_lit";
constexpr char const* kFloatingItem = "bedstones:visuals_floating_item";
constexpr char const* kPortal       = "minecraft:portal_reverse_particle";
constexpr char const* kEnchant      = "minecraft:enchanting_table_particle";

constexpr int   kRuneEvery     = 10;   ///< ticks between rune respawns
constexpr float kRuneLife      = 0.6f; ///< seconds (overlaps the next respawn)
constexpr int   kFloatEvery    = 20;
constexpr float kFloatLife     = 1.1f;
constexpr int   kPortstoneScan = 40;   ///< ticks between portstone scans
constexpr int   kScanRadius    = 12;
constexpr int   kScanHeight    = 6;

struct Portstone {
    core::GlobalPos pos;
    std::uint32_t   color;
};

struct State {
    std::vector<cv::Entry>          entries;
    std::vector<Portstone>          portstones;
    std::unordered_set<std::string> rendering; ///< uids whose runes are currently drawn (for logs)
    int                             ticks = 0;
    std::mt19937                    rng{0xB5u};
};
State gState; // client thread

void reset() { gState = State{}; }

void spawnRunes(Level& level, core::GlobalPos const& pos, cv::RuneKind kind, std::uint32_t color) {
    auto const* effect = config::get<ClientConfig>().disableTextGlow ? kRuneLit : kRuneGlow;
    float       r = ((color >> 16) & 0xFF) / 255.f, g = ((color >> 8) & 0xFF) / 255.f, b = (color & 0xFF) / 255.f;
    float       u = static_cast<float>(static_cast<int>(kind) * cv::kRuneCellWidth);
    for (auto const& q : cv::runeQuads(pos)) {
        spawnLocal(
            level,
            effect,
            Vec3{static_cast<float>(q.x), static_cast<float>(q.y), static_cast<float>(q.z)},
            {
                {"bs_nx",   static_cast<float>(q.nx)},
                {"bs_nz",   static_cast<float>(q.nz)},
                {"bs_r",    r                       },
                {"bs_g",    g                       },
                {"bs_b",    b                       },
                {"bs_u",    u                       },
                {"bs_life", kRuneLife               },
        }
        );
    }
}

void spawnFloatingItem(Level& level, core::GlobalPos const& pos, std::uint64_t gameTime) {
    // The particle animates itself from these start values (angle = t/2 degrees, bob =
    // sin(t/8) * 0.025 with t = start + age in ticks); see cv::floatingPose.
    float spin0 = static_cast<float>(gameTime % 720);
    float bob0  = static_cast<float>(std::fmod(static_cast<double>(gameTime), 16.0 * 3.14159265358979));
    Vec3  at{pos.x + 0.5f, pos.y + static_cast<float>(cv::kFloatBaseY), pos.z + 0.5f};
    // Two back-to-back quads: particle quads are single-sided.
    for (float side : {1.f, -1.f}) {
        spawnLocal(
            level,
            kFloatingItem,
            at,
            {
                {"bs_spin0", spin0                                  },
                {"bs_bob0",  bob0                                   },
                {"bs_side",  side                                   },
                {"bs_size",  static_cast<float>(cv::kFloatScale / 2)},
                {"bs_life",  kFloatLife                             },
        }
        );
    }
}

void ambient(Level& level, cv::Entry const& e) {
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    if (unit(gState.rng) >= cv::kAmbientChance) return;
    auto at = [&] {
        return Vec3{
            e.pos.x + 0.5f + (unit(gState.rng) - 0.5f) * 1.5f,
            e.pos.y + 0.5f,
            e.pos.z + 0.5f + (unit(gState.rng) - 0.5f) * 1.5f
        };
    };
    spawnLocal(level, kPortal, at());
    spawnLocal(level, kEnchant, at());
}

/// Finds portstones (lower halves) around the player. Skipped while the block is unknown.
void scanPortstones(LocalPlayer& player) {
    gState.portstones.clear();
    if (!game::blockWith(ids::id(ids::portstonePath("white")))) return;
    auto&    region = player.getDimensionBlockSource();
    int      dim    = static_cast<int>(player.getDimensionId());
    auto     feet   = player.getFeetBlockPos();
    BlockPos p;
    for (p.x = feet.x - kScanRadius; p.x <= feet.x + kScanRadius; ++p.x) {
        for (p.z = feet.z - kScanRadius; p.z <= feet.z + kScanRadius; ++p.z) {
            for (p.y = feet.y - kScanHeight; p.y <= feet.y + kScanHeight; ++p.y) {
                auto const& block = region.getBlock(p);
                auto        path  = ids::pathOf(block.getTypeName());
                if (!path.ends_with("_portstone") || game::intState(block, ids::kStateUpper).value_or(0) != 0) continue;
                gState.portstones.push_back({game::globalPos(dim, p), cv::dyeRuneColor(path)});
            }
        }
    }
}

void tick(Level& level) {
    auto* player = currentPlayer();
    if (!player) return;
    ++gState.ticks;
    auto pos = player->getPosition();
    int  dim = static_cast<int>(player->getDimensionId());

    if (gState.ticks % kPortstoneScan == 1) scanPortstones(*player);
    bool runes    = gState.ticks % kRuneEvery == 0;
    bool floating = gState.ticks % kFloatEvery == 0;
    auto gameTime = static_cast<std::uint64_t>(level.getCurrentTick().tickID);

    for (auto const& e : gState.entries) {
        if (cv::hasAmbientParticles(e) && cv::inRange(e.pos, dim, pos.x, pos.y, pos.z, cv::kAmbientRange)) ambient(level, e);
        if (!cv::inRange(e.pos, dim, pos.x, pos.y, pos.z, cv::kRenderRange)) continue;
        if (runes && cv::showsRunes(e)) {
            auto kind = e.type == core::types::kWaystone ? cv::RuneKind::Waystone : cv::RuneKind::Sharestone;
            spawnRunes(level, e.pos, kind, cv::runeColor(e));
            if (gState.rendering.insert(e.uid).second) {
                log().info("visuals: drawing runes of {} {} (colour {:06X})", e.style, e.uid, cv::runeColor(e));
            }
        }
        if (floating && cv::hasFloatingItem(e)) spawnFloatingItem(level, e.pos, gameTime);
    }
    if (runes) {
        for (auto const& ps : gState.portstones) {
            if (ps.pos.dim == dim) spawnRunes(level, ps.pos, cv::RuneKind::Portstone, ps.color);
        }
    }
}

void onPayload(nlohmann::json const& data) {
    gState.entries = cv::fromJson(data);
    std::unordered_set<std::string> still;
    std::size_t                     runes = 0;
    for (auto const& e : gState.entries) {
        if (!cv::showsRunes(e)) continue;
        ++runes;
        if (gState.rendering.contains(e.uid)) still.insert(e.uid);
    }
    gState.rendering = std::move(still);
    log().info("visuals: client has {} waystones ({} with runes)", gState.entries.size(), runes);
}

} // namespace

class WaystoneVisualsModule {
public:
    void install() {
        mTick = ll::event::EventBus::getInstance().emplaceListener<ll::event::ClientLevelTickEvent>(
            [](ll::event::ClientLevelTickEvent& ev) { tick(ev.level()); }
        );
    }
    void uninstall() {
        ll::event::EventBus::getInstance().removeListener(mTick);
        mTick.reset();
        reset();
    }

private:
    ll::event::ListenerPtr mTick;
};

namespace {
struct LeaveHook {
    LeaveHook() { net::onLeaveWorld([] { reset(); }); }
} gLeaveHook;
} // namespace

} // namespace bs::client::render

BS_REGISTER_CONFIG("client", bs::client::render::ClientConfig);
BS_REGISTER_SERVER_MESSAGE("visuals.waystones", [](nlohmann::json const& data) { bs::client::render::onPayload(data); });
BS_REGISTER_MODULE(bs::client::render::WaystoneVisualsModule);

#endif // BS_CLIENT
