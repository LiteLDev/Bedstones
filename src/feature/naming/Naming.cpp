// U9: the INameGenerator seam backed by core::naming::NameGenerator, persisted per world.

#include "feature/naming/Naming.h"

#include <random>

#include "feature/access/GeneralConfig.h"
#include "game/Waystones.h"
#include "game/WorldDb.h"
#include "mod/Config.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "service/Services.h"

namespace bs::feature::naming {

namespace {

constexpr std::string_view kNamesKey = "bs:names";

struct State {
    bool                        installed = false;
    bool                        loaded    = false;
    core::naming::NameGenerator generator;
    std::mt19937                engine{std::random_device{}()};
};
State& state() {
    static State s;
    return s;
}

core::naming::NameGenerator& generator() {
    auto& s = state();
    if (!s.loaded && game::db::available()) {
        s.loaded = true;
        if (auto doc = game::db::load(std::string(kNamesKey))) s.generator.loadJson(*doc);
        else s.generator.clear();
        // Names of existing waystones count as used too (worlds from before this unit, renames).
        for (auto* r : game::Waystones::get().all()) {
            if (!r->name.empty()) s.generator.markUsed(r->name);
        }
    }
    return s.generator;
}

void save() {
    if (state().loaded && game::db::available()) game::db::save(std::string(kNamesKey), state().generator.toJson());
}

class Names final : public service::INameGenerator {
public:
    std::string generate(core::Origin origin, std::string const& biome) override {
        return naming::generate(modeFor(origin), biome);
    }
};

class NamingModule {
public:
    void install() {
        auto& s = state();
        s.installed = true;
        static bool registered = false;
        if (!registered) {
            registered = true;
            game::db::onWorldReady([] { state().loaded = false; });
            game::db::onWorldClosed([] {
                state().loaded = false;
                state().generator.clear();
            });
        }
        service::set<service::INameGenerator>(std::make_unique<Names>());
    }
    void uninstall() {
        service::reset<service::INameGenerator>();
        state().installed = false;
    }
};

} // namespace

core::naming::Mode modeFor(core::Origin origin) {
    switch (origin) {
    case core::Origin::Wilderness:
    case core::Origin::Village:
    case core::Origin::Dungeon:
        return core::naming::modeFromString(config::get<access::GeneralConfig>().nameGenerationMode)
            .value_or(core::naming::Mode::PresetFirst);
    default:
        return core::naming::Mode::RandomOnly;
    }
}

std::string generate(core::naming::Mode mode, std::string const& biome) {
    auto const&            cfg = config::get<access::GeneralConfig>();
    core::naming::Settings settings{mode, cfg.nameGenerationTemplate, cfg.nameGenerationPresets};
    auto&                  engine = state().engine;
    auto name = generator().generate(settings, biome, [&engine](std::uint32_t bound) {
        return std::uniform_int_distribution<std::uint32_t>(0, bound - 1)(engine);
    });
    save();
    log().info("name generated: {} ({})", name, core::naming::toString(mode));
    return name;
}

bool isUsed(std::string const& name) { return generator().usedNames().contains(name); }

void forget(std::string const& name) {
    if (generator().forget(name)) save();
}

} // namespace bs::feature::naming

BS_REGISTER_MODULE(bs::feature::naming::NamingModule);
