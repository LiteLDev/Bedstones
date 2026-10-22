#pragma once

// Unit U3: the Warp Plate block and its shards (docs/SPEC.md §1.5, §2, §3.10).
//
// The plate is a custom block (`bedstones:warp_plate`) with the integer states
// `bedstones:status` (ids::WarpPlateStatus) and `bedstones:plate_color` (0..14, the galactic
// colour index that tints the runes). Everything else lives in its waystone record (type
// `warp_plate`, visibility ShardOnly): the 5-slot container in `extra["container"]`
// (game/BlockContainer.h). Per-entity timers and cooldowns are runtime only, like Java's.

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ll/api/event/ListenerBase.h"

#include "core/Waystone.h"
#include "game/BlockContainer.h"
#include "game/Items.h"

class Player;
class Actor;
class ItemStack;
class BlockSource;
class BlockPos;

namespace bs::feature::warpplate {

/// Config section `warpPlate` (Waystones' general.warpPlateUseTime / warpPlateCooldownTime).
struct WarpPlateConfig {
    int version               = 1;
    int warpPlateUseTime      = 15; ///< ticks an entity has to stand on the plate
    int warpPlateCooldownTime = 20; ///< ticks off the arrival plate before it sends the entity back
};

inline constexpr std::string_view kColorState = "bedstones:plate_color";

[[nodiscard]] std::string plateBlockId();
[[nodiscard]] bool        isPlateBlock(std::string_view blockId);

/// "§<colour>" + galactic identifier of a plate uid (Bedrock has no Standard Galactic font).
[[nodiscard]] std::string galacticName(std::string const& uid);
/// The tooltip lines of an attuned / crumbling shard bound to `target` (localised for `player`).
[[nodiscard]] std::vector<std::string>
shardLore(Player const* player, std::string_view itemId, std::string const& target);
/// A fresh attuned shard bound to `target` (lore in the default locale).
[[nodiscard]] ItemStack makeAttunedShard(std::string const& target, bool crumbling = false);

/// How a player broke a plate (PlayerDestroyBlockEvent), consumed by the block-change handler.
struct BreakInfo {
    bool          creative = false;
    bool          pickaxe  = false;
    bool          silk     = false;
    std::uint64_t tick     = 0;
};

class WarpPlates {
public:
    static WarpPlates& get();

    void install();
    void uninstall();

    // ---- actions (server thread; also used by selftests) -------------------------------------
    /// Creates (or restores, for a silk-touched item uid) the record of a plate block at `pos`.
    /// A fresh player-placed plate starts with a dormant shard. Returns the record.
    core::WaystoneRecord*
    onPlaced(Player* placer, core::GlobalPos const& pos, std::optional<std::string> const& itemUid, bool dormantShard);
    /// Inserts one shard from `loc` into the empty shard slot (creative keeps it).
    bool insertShard(Player& player, std::string const& uid, game::ItemLocation loc);
    /// Drops the shard slot as an item above the plate.
    bool ejectShard(std::string const& uid);
    /// The block at the record's position was removed (drops + record removal / transient).
    void onRemoved(core::WaystoneRecord const& record, std::optional<BreakInfo> info);
    /// Remembers a player break at `pos` (consumed when the block disappears).
    void noteBreak(core::GlobalPos const& pos, BreakInfo info);

    /// Use time of a plate with its modifiers (amethyst / slime ball).
    [[nodiscard]] int useTimeOf(core::WaystoneRecord const& record) const;
    /// The plate's desired status right now (ids::WarpPlateStatus value).
    [[nodiscard]] int statusOf(std::string const& uid) const;

    /// Entity `actorId` just arrived at plate `uid`: it can't be sent back before stepping off
    /// for `warpPlateCooldownTime` ticks.
    void               markCooldown(std::string const& uid, std::int64_t actorId);
    [[nodiscard]] bool onCooldown(std::string const& uid, std::int64_t actorId) const;

    [[nodiscard]] std::uint64_t now() const { return mNow; }

private:
    struct Runtime {
        int                          attune   = 0;
        int                          lastSlot = 0;
        int                          status   = 0;
        std::map<std::int64_t, int>  sessions;  ///< entity -> ticks on the plate
        std::map<std::int64_t, bool> valid;     ///< entity -> target valid when it stepped on
        std::map<std::int64_t, int>  cooldowns; ///< entity -> remaining off-plate ticks
    };
    struct Pending {
        enum class Kind { Insert, Eject, Edit } kind;
        std::int64_t player;
        std::string  uid;
        int          slot = 0;
    };
    struct Drop {
        core::GlobalPos             pos;
        std::vector<game::SlotItem> items;
    };
    struct Placing {
        std::int64_t               player;
        std::optional<std::string> uid;
        std::uint64_t              tick;
    };

    void tick();
    void tickPlate(std::string const& uid);
    void runPending();
    void spawnDrops();
    void refreshLore();
    void checkOrphans();
    bool warpEntity(Actor& actor, std::string const& uid);

    std::uint64_t                            mNow = 0;
    std::unordered_map<std::string, Runtime> mRt;
    std::vector<Pending>                     mPending;
    std::vector<Drop>                        mDrops;
    std::vector<core::GlobalPos>             mOrphans;
    std::map<std::string, BreakInfo>         mBreaks;   ///< pos key -> player break
    std::map<std::int64_t, Placing>          mPlacing;  ///< player -> held plate item
    std::map<std::int64_t, std::uint64_t>    mThrottle; ///< player -> last interaction tick
    std::vector<ll::event::ListenerPtr>      mListeners;
    std::vector<Player*>                     mTickPlayers; ///< online players, valid during one tick
};

} // namespace bs::feature::warpplate
