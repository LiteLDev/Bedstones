#ifdef BS_CLIENT

#include "client/render/ClientParticles.h"

#include <cstddef>
#include <memory>
#include <string>

#include "mc/deps/core/math/Vec3.h"
#include "mc/deps/core/string/HashedString.h"
#include "mc/util/MolangScriptArg.h"
#include "mc/util/MolangVariableMap.h"
#include "mc/world/level/Level.h"

namespace bs::client::render {

namespace {

/// A MolangVariableMap built in place: its default constructor is not exported, but an empty map
/// is just two empty vectors and a false flag. Destroyed with the game's destructor.
class LocalMolangMap {
public:
    LocalMolangMap() {
        auto* m = map();
        std::construct_at(std::addressof(m->mMapFromVariableIndexToVariableArrayOffset));
        std::construct_at(std::addressof(m->mVariables));
        m->mHasPublicVariables = false;
    }
    ~LocalMolangMap() { map()->~MolangVariableMap(); }
    LocalMolangMap(LocalMolangMap const&)            = delete;
    LocalMolangMap& operator=(LocalMolangMap const&) = delete;

    MolangVariableMap* map() { return reinterpret_cast<MolangVariableMap*>(mStorage); }

private:
    alignas(MolangVariableMap) std::byte mStorage[sizeof(MolangVariableMap)];
};

} // namespace

void spawnLocal(Level& clientLevel, std::string_view effect, Vec3 const& at, std::initializer_list<MolangVar> vars) {
    LocalMolangMap vm;
    for (auto const& [name, value] : vars) {
        vm.map()->setMolangVariable(HashedString("variable." + std::string(name)), MolangScriptArg(value));
    }
    clientLevel.addParticleEffect(HashedString(effect), at, *vm.map());
}

} // namespace bs::client::render

#endif // BS_CLIENT
