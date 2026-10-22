#pragma once

// Warp modifiers (docs/SPEC.md §3.10, §4.6), pure logic: the effects a waystone / warp plate
// container applies to arriving entities, the "N modifiers active" count, the galactic name of a
// block and the server-authoritative click rules of the Warp Modifiers menu.

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bs::core::modifiers {

// ---- items ----------------------------------------------------------------------------------------

/// An item stack as the menu sees it (`maxStack` = the item's own stack size).
struct Stack {
    std::string                        id;
    int                                count    = 1;
    int                                aux      = 0;
    std::map<std::string, std::string> user;
    int                                maxStack = 64;

    /// Same item and data (stackable with each other).
    [[nodiscard]] bool sameItem(Stack const& o) const { return id == o.id && aux == o.aux && user == o.user; }
    [[nodiscard]] bool operator==(Stack const&) const = default;
};
using Slot = std::optional<Stack>;

inline constexpr std::string_view kDiamond = "minecraft:diamond";

/// Items of `#waystones:warp_shards` (the only items the shard slot 0 accepts).
[[nodiscard]] bool isWarpShard(std::string_view id);

// ---- effects ----------------------------------------------------------------------------------------

enum class Effect { Poison, Blindness, SlowFalling, FireResistance, Wither };

struct EffectSpec {
    Effect effect;
    int    seconds   = 0;
    int    amplifier = 0;

    [[nodiscard]] bool operator==(EffectSpec const&) const = default;
};

struct ArrivalEffects {
    bool                    cure        = false; ///< milk bucket / honey block: remove all effects first
    int                     fireSeconds = 0;     ///< blaze powder: set on fire
    std::vector<EffectSpec> effects;             ///< in table order

    [[nodiscard]] bool empty() const { return !cure && fireSeconds <= 0 && effects.empty(); }
};

/// Seconds an item kind may add at most (slow falling and fire resistance: 8 s).
inline constexpr int kMaxShortEffectSeconds = 8;
inline constexpr int kMaxPotency            = 4;

/// Potency of the modifier slots: 1 + diamonds, at most 4 (used as the effect amplifier).
[[nodiscard]] int potency(std::vector<Stack> const& modifiers);
/// What arriving entities get from the modifier slots (1-4) of a container.
[[nodiscard]] ArrivalEffects arrivalEffects(std::vector<Stack> const& modifiers);
/// "N modifiers active": every item in the modifier slots except amethyst shards and slime balls
/// (which only change warp plate timing).
[[nodiscard]] int activeModifierCount(std::vector<Stack> const& modifiers);

// ---- galactic name ------------------------------------------------------------------------------------

/// Java `String.hashCode`.
[[nodiscard]] std::int32_t javaHash(std::string_view s);
/// The uid without digits and dashes, first 8 characters (Standard Galactic Alphabet text).
[[nodiscard]] std::string galacticIdentifier(std::string_view uid);
/// ChatFormatting name of the identifier's colour: getById(|hash| % 15) with gray -> light_purple,
/// dark_gray -> dark_purple, black -> gold.
[[nodiscard]] std::string_view galacticColor(std::string_view identifier);

// ---- the Warp Modifiers menu ----------------------------------------------------------------------------

inline constexpr int kContainerSlots = 5;
inline constexpr int kInventorySlots = 36; ///< player inventory, 0..8 hotbar
/// Menu slot ids: 0..4 container, kInventoryBase + i = player inventory slot i.
inline constexpr int kInventoryBase = 5;
inline constexpr int kOutside       = -999; ///< click outside the panel: drop the cursor item

struct Menu {
    std::array<Slot, kContainerSlots> container{};
    std::array<Slot, kInventorySlots> inventory{};
    Slot                              cursor;
};

/// The slot a menu slot id names (nullptr if out of range).
[[nodiscard]] Slot*       slotOf(Menu& menu, int slotId);
/// Whether `stack` may be put into menu slot `slotId` (slot 0: warp shards only).
[[nodiscard]] bool        mayPlace(int slotId, Stack const& stack);
/// How many of `stack` slot `slotId` can hold (slot 0: 1).
[[nodiscard]] int         slotLimit(int slotId, Stack const& stack);

/// A left (button 0) / right (button 1) click on a slot (Java PICKUP). `dropped` receives what a
/// click outside the panel throws away. Returns true when something changed.
bool click(Menu& menu, int slotId, int button, std::vector<Stack>* dropped = nullptr);
/// Shift-click (Java QUICK_MOVE, repeated while it moves something): container -> player
/// inventory (hotbar first, from the end); inventory -> one item into the empty shard slot when it
/// fits, otherwise into slots 1-4. Returns true when something moved.
bool quickMove(Menu& menu, int slotId);
/// Puts the cursor item back into the player inventory (equal stacks first, then free slots, like
/// Java's `placeItemBackInInventory`). Returns what did not fit (nullopt when everything did).
Slot returnCursor(Menu& menu);

} // namespace bs::core::modifiers
