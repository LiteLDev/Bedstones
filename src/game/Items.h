#pragma once

// Item helpers (server thread): stack construction, user-data strings, player inventory access.

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class ItemStack;
class Player;

namespace bs::game {

/// Item id of a stack ("" when empty), e.g. "bedstones:warp_scroll".
[[nodiscard]] std::string idOf(ItemStack const& stack);

/// A plain stack of `id` (no NBT). Empty if the id is unknown. `count` is clamped to 1..255.
[[nodiscard]] ItemStack makeItem(std::string_view id, int count = 1, int aux = 0);

/// Sets the custom name and lore shown by the client (empty name keeps the default name).
void setDisplay(ItemStack& stack, std::string const& name, std::vector<std::string> const& lore = {});

/// String value stored in the stack's user data (custom NBT), nullopt when absent or empty.
[[nodiscard]] std::optional<std::string> userString(ItemStack const& stack, std::string_view key);
/// Stores (or removes, with an empty value) a string in the stack's user data.
void setUserString(ItemStack& stack, std::string_view key, std::string const& value);

/// Where an item sits on a player.
struct ItemLocation {
    enum class Kind {
        Inventory, ///< `slot` = inventory index 0..35 (0..8 hotbar)
        Offhand,   ///< `slot` unused (0)
    };
    Kind kind = Kind::Inventory;
    int  slot = 0;

    [[nodiscard]] bool operator==(ItemLocation const&) const = default;
};

/// The item at `loc` (an empty stack if out of range).
[[nodiscard]] ItemStack const& itemAt(Player& player, ItemLocation loc);
/// Replaces the item at `loc` and syncs it to the client.
void setItemAt(Player& player, ItemLocation loc, ItemStack const& item);
/// The selected hotbar slot (hand 0) or the offhand (hand 1).
[[nodiscard]] ItemLocation handSlot(Player& player, int hand = 0);
/// First location (inventory, then offhand) whose item matches `pred`.
[[nodiscard]] std::optional<ItemLocation> findItem(Player& player, std::function<bool(ItemStack const&)> const& pred);
/// Removes `count` items from the stack at `loc` (no-op in creative when `respectCreative`).
void consumeAt(Player& player, ItemLocation loc, int count = 1, bool respectCreative = true);
/// Adds `item` to the inventory, dropping what does not fit at the player's feet.
void giveOrDrop(Player& player, ItemStack const& item);

} // namespace bs::game
