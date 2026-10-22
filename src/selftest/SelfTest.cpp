#include "selftest/SelfTest.h"

#include <algorithm>

#include "ll/api/chrono/GameChrono.h"
#include "ll/api/coro/CoroTask.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/thread/ServerThreadExecutor.h"

#include "mc/server/SimulatedPlayer.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/BlockPos.h"
#include "mc/world/level/BlockSource.h"
#include "mc/world/level/Level.h"
#include "mc/world/level/dimension/Dimension.h"

#include "game/Blocks.h"
#include "game/Waystones.h"
#include "mod/Log.h"
#include "net/Screens.h"

namespace bs::selftest {

namespace {
bool gRunning       = false;
int  gPlayerCounter = 0;

BlockSource* overworld() {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    auto dim = level->getDimension(0).lock();
    if (!dim) return nullptr;
    return &dim->getBlockSourceFromMainChunkSource();
}
} // namespace

Context::Context(std::string name) : mName(std::move(name)) {}

Context::~Context() {
    for (auto* p : mPlayers) {
        if (!p) continue;
        net::screens::close(*p);
        p->simulateDisconnect();
    }
    auto& ws = game::Waystones::get();
    for (auto const& uid : mWaystones) ws.remove(uid);
    if (auto* region = overworld()) {
        for (auto const& pos : mBlocks) game::clearBlock(*region, game::blockPos(pos));
    }
}

SimulatedPlayer& Context::spawnPlayer(std::string const& name) {
    auto level = ll::service::getLevel();
    if (!level) fail("no level loaded");
    auto spawn = level->getSharedSpawnPos();
    auto pos   = Vec3{static_cast<float>(spawn.x) + 0.5f, static_cast<float>(spawn.y) + 1.0f, static_cast<float>(spawn.z) + 0.5f};
    auto n     = name.empty() ? "bs_test_" + std::to_string(++gPlayerCounter) : name;
    auto sp    = SimulatedPlayer::create(n, pos, 0);
    if (!sp) fail("could not spawn a simulated player");
    mPlayers.push_back(sp.as_ptr());
    return *sp;
}

Player& Context::player() {
    if (mPlayers.empty()) return spawnPlayer();
    return *mPlayers.front();
}

core::GlobalPos Context::testPos(int dx, int dz) const {
    auto level = ll::service::getLevel();
    if (!level) return {};
    auto spawn = level->getSharedSpawnPos();
    return {0, spawn.x + dx, spawn.y, spawn.z + dz};
}

core::WaystoneRecord& Context::createWaystone(core::WaystoneRecord record) {
    auto* r = game::Waystones::get().create(std::move(record));
    if (!r) fail("could not create a waystone record (no level?)");
    mWaystones.push_back(r->uid);
    return *r;
}

bool Context::setBlock(core::GlobalPos const& pos, std::string const& id, std::vector<std::pair<std::string, int>> const& states) {
    auto* region = overworld();
    if (!region) return false;
    std::vector<std::pair<std::string_view, int>> s;
    for (auto const& [k, v] : states) s.emplace_back(k, v);
    auto const* block = game::blockWith(id, s);
    if (!block) return false;
    mBlocks.push_back(pos);
    return game::setBlock(*region, game::blockPos(pos), *block);
}

void Context::cleanupBlock(core::GlobalPos const& pos) { mBlocks.push_back(pos); }

void Context::cleanupWaystone(std::string const& uid) { mWaystones.push_back(uid); }

int Context::give(Player& player, ItemStack const& item, int slot) {
    auto& inv = player.getInventory();
    if (slot < 0) {
        for (int i = 0; i < inv.getContainerSize(); ++i) {
            if (inv.getItem(i).isNull()) {
                slot = i;
                break;
            }
        }
    }
    if (slot < 0 || slot >= inv.getContainerSize()) return -1;
    inv.setItem(slot, item);
    player.sendInventory(false);
    return slot;
}

ll::coro::CoroTask<> Context::ticks(int n) { co_await ll::chrono::ticks(std::max(n, 1)); }

void Context::check(bool condition, std::string const& message) {
    if (!condition) mFailures.push_back(message);
}

void Context::require(bool condition, std::string const& message) {
    if (!condition) fail(message);
}

void Context::fail(std::string const& message) {
    mFailures.push_back(message);
    throw Abort(message);
}

void Context::info(std::string const& message) const { log().info("SELFTEST INFO {}: {}", mName, message); }

bool running() { return gRunning; }

void runAll(std::string const& glob, std::function<void(std::string const&)> out) {
    if (gRunning) {
        out("SELFTEST BUSY");
        return;
    }
    std::vector<TestEntry const*> tests;
    for (auto const& t : TestRegistry::entries()) {
        if (glob.empty() || core::globMatch(glob, t.name)) tests.push_back(&t);
    }
    std::sort(tests.begin(), tests.end(), [](auto* a, auto* b) { return a->name < b->name; });
    gRunning  = true;
    auto emit = [out](std::string const& line) {
        log().info("{}", line);
        if (out) out(line);
    };
    emit("SELFTEST START count=" + std::to_string(tests.size()));
    ll::coro::keepThis([tests = std::move(tests), emit]() -> ll::coro::CoroTask<> {
        int pass = 0, fail = 0;
        for (auto const* t : tests) {
            std::string reason;
            {
                Context ctx(t->name);
                auto    result = co_await t->run(ctx).tryGet();
                if (!ctx.failures().empty()) {
                    for (auto const& f : ctx.failures()) reason += (reason.empty() ? "" : "; ") + f;
                } else if (!result) {
                    reason = "exception: " + result.error().message();
                }
            } // cleanup (players, waystones, blocks) before the next test
            if (reason.empty()) {
                ++pass;
                emit("SELFTEST PASS " + t->name);
            } else {
                ++fail;
                emit("SELFTEST FAIL " + t->name + ": " + reason);
            }
            co_await ll::chrono::ticks(1);
        }
        emit("SELFTEST DONE pass=" + std::to_string(pass) + " fail=" + std::to_string(fail));
        gRunning = false;
    }).launch(ll::thread::ServerThreadExecutor::getDefault());
}

} // namespace bs::selftest
