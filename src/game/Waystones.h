#pragma once

// The world's waystone registry and per-player waystone data (server thread only).
//
// Records (any type: waystones, sharestones, warp plates, portals, memorials) are kept in memory
// and persisted as one JSON document under the LevelDB key "bs:waystones". Player data lives
// under "bs:player:<player uuid>". Both load lazily on first access after a world is ready and
// are written on autosave / world stop. Record pointers stay valid until the record is removed
// or the world closes: never keep them across ticks, keep the uid.
//
// Mutations go through this class (or are followed by `markDirty`) so listeners see them:
// the network sync, map-like features and selftests subscribe with `onChanged`.

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Waystone.h"

class Player;

namespace bs::game {

enum class WaystoneChange {
    Created,
    Updated,  ///< name / visibility / owner / extra changed (`markDirty`)
    Removed,  ///< the record is about to be deleted (still readable in the callback)
};

/// What changed for a player (`onPlayerChanged`).
enum class PlayerChange {
    Activated,
    Forgotten,
    DataUpdated, ///< any other player data change (`markPlayerDirty`)
};

class Waystones {
public:
    static Waystones& get();

    /// A world is loaded and the registry can be used.
    [[nodiscard]] bool available() const;

    // ---- records ------------------------------------------------------------------------------
    /// Creates a record. A missing uid gets a random one; an existing uid is replaced. Returns the
    /// stored record, nullptr when no world is loaded.
    core::WaystoneRecord* create(core::WaystoneRecord record);
    [[nodiscard]] core::WaystoneRecord* find(std::string_view uid);
    /// The record whose block is at `pos` (lower half), nullptr if none.
    [[nodiscard]] core::WaystoneRecord* findAt(core::GlobalPos const& pos);
    /// Records matching `pred` (all when null), in creation order.
    [[nodiscard]] std::vector<core::WaystoneRecord*> all(std::function<bool(core::WaystoneRecord const&)> const& pred = {});
    /// Records of a type ("waystone", "red_sharestone", ...).
    [[nodiscard]] std::vector<core::WaystoneRecord*> ofType(std::string_view type);
    /// Moves a record's position (e.g. the silk-touched item placed again elsewhere).
    void move(std::string_view uid, core::GlobalPos const& pos);
    /// Persists `uid` and notifies `Updated` listeners. Call after editing a record in place.
    void markDirty(std::string_view uid);
    /// Removes a record (players forget it lazily: unknown uids are dropped from lists on read).
    void remove(std::string_view uid);

    // ---- players --------------------------------------------------------------------------------
    /// The player's UUID string (key of the player data).
    [[nodiscard]] static std::string playerKey(Player const& player);
    /// Player data, created empty on first access. Unknown (removed) waystone uids are pruned.
    core::PlayerWaystoneData& playerData(Player const& player);
    core::PlayerWaystoneData& playerData(std::string const& playerUuid);
    /// Persists a player's data and notifies `DataUpdated`.
    void markPlayerDirty(std::string const& playerUuid);

    [[nodiscard]] bool isActivated(Player const& player, std::string_view uid);
    /// Activates `uid` for the player. Returns false if it was already activated / unknown.
    bool activate(Player const& player, std::string const& uid);
    /// Forgets (deactivates) `uid` for the player. Returns false if it was not activated.
    bool forget(Player const& player, std::string const& uid);
    /// Every player uuid with stored data in this world (online or not).
    [[nodiscard]] std::vector<std::string> knownPlayers();

    // ---- listeners -------------------------------------------------------------------------------
    using RecordListener = std::function<void(WaystoneChange, core::WaystoneRecord const&)>;
    using PlayerListener = std::function<void(PlayerChange, std::string const& playerUuid, std::string const& waystoneUid)>;
    /// Adds a listener; returns an id for `removeListener` (listeners added at install time can
    /// stay for the whole session).
    int  onChanged(RecordListener fn);
    int  onPlayerChanged(PlayerListener fn);
    void removeListener(int id);

    // ---- internals (module) ----------------------------------------------------------------------
    void saveDirty();
    void reset();

private:
    void ensureLoaded();
    void notify(WaystoneChange change, core::WaystoneRecord const& r);
    void notifyPlayer(PlayerChange change, std::string const& player, std::string const& uid);

    struct Impl;
    Impl* mImpl;
    Waystones();
};

} // namespace bs::game
