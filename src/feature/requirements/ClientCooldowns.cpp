#ifdef BS_CLIENT
// Client mod side of the cooldown sync: keeps the cooldowns the server sent on the
// "requirements.cooldowns" channel (see Requirements.h) as local expiry times.

#include <chrono>
#include <map>
#include <string>

#include "client/net/ClientNet.h"
#include "feature/requirements/Requirements.h"
#include "mod/Log.h"

namespace bs::feature::requirements {

namespace {
using Clock = std::chrono::steady_clock;
std::map<std::string, Clock::time_point>& expiries() {
    static std::map<std::string, Clock::time_point> m;
    return m;
}

struct LeaveWorldReset {
    LeaveWorldReset() {
        client::net::onLeaveWorld([] { expiries().clear(); });
    }
} gLeaveWorldReset;
} // namespace

std::int64_t clientCooldownRemainingMs(std::string const& id) {
    auto it = expiries().find(id);
    if (it == expiries().end()) return 0;
    auto left = std::chrono::duration_cast<std::chrono::milliseconds>(it->second - Clock::now()).count();
    return left > 0 ? left : 0;
}

} // namespace bs::feature::requirements

BS_REGISTER_SERVER_MESSAGE(std::string(bs::feature::requirements::kCooldownChannel), [](nlohmann::json const& data) {
    using namespace bs::feature::requirements;
    auto& m = expiries();
    m.clear();
    if (!data.is_object()) return;
    auto now = Clock::now();
    for (auto const& [id, ms] : data.items()) {
        if (ms.is_number() && ms.get<std::int64_t>() > 0) m[id] = now + std::chrono::milliseconds(ms.get<std::int64_t>());
    }
    bs::log().info("requirements: client received {} cooldown(s)", m.size());
});
#endif
