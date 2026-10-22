#include "game/Items.h"

#include <algorithm>

#include "mc/deps/nbt/CompoundTag.h"
#include "mc/safety/RedactableString.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStack.h"

namespace bs::game {

std::string idOf(ItemStack const& stack) {
    if (stack.isNull()) return {};
    return stack.getTypeName();
}

ItemStack makeItem(std::string_view id, int count, int aux) {
    ItemStack s;
    s.reinit(id, std::clamp(count, 1, 255), aux);
    return s;
}

void setDisplay(ItemStack& stack, std::string const& name, std::vector<std::string> const& lore) {
    if (stack.isNull()) return;
    if (!name.empty()) stack.setCustomName(Bedrock::Safety::RedactableString(name));
    if (!lore.empty()) stack.setCustomLore(lore);
}

std::optional<std::string> userString(ItemStack const& stack, std::string_view key) {
    if (stack.isNull() || !stack.mUserData) return std::nullopt;
    auto const& tag = *stack.mUserData;
    if (!tag.contains(key, Tag::Type::String)) return std::nullopt;
    std::string const& s = tag[key];
    if (s.empty()) return std::nullopt;
    return s;
}

void setUserString(ItemStack& stack, std::string_view key, std::string const& value) {
    if (stack.isNull()) return;
    auto tag = stack.mUserData ? stack.mUserData->clone() : std::make_unique<CompoundTag>();
    if (value.empty()) tag->erase(key);
    else tag->putString(std::string(key), value);
    stack.setUserData(std::move(tag));
}

ItemStack const& itemAt(Player& player, ItemLocation loc) {
    switch (loc.kind) {
    case ItemLocation::Kind::Inventory: {
        auto& inv = player.getInventory();
        if (loc.slot < 0 || loc.slot >= inv.getContainerSize()) return ItemStack::EMPTY_ITEM();
        return inv.getItem(loc.slot);
    }
    case ItemLocation::Kind::Offhand:
        return player.getOffhandSlot();
    }
    return ItemStack::EMPTY_ITEM();
}

void setItemAt(Player& player, ItemLocation loc, ItemStack const& item) {
    switch (loc.kind) {
    case ItemLocation::Kind::Inventory: {
        auto& inv = player.getInventory();
        if (loc.slot < 0 || loc.slot >= inv.getContainerSize()) return;
        inv.setItem(loc.slot, item);
        player.sendInventory(false);
        break;
    }
    case ItemLocation::Kind::Offhand:
        player.setOffhandSlot(item);
        break;
    }
}

ItemLocation handSlot(Player& player, int hand) {
    if (hand == 1) return {ItemLocation::Kind::Offhand, 0};
    return {ItemLocation::Kind::Inventory, player.getSelectedItemSlot()};
}

std::optional<ItemLocation> findItem(Player& player, std::function<bool(ItemStack const&)> const& pred) {
    int size = player.getInventory().getContainerSize();
    for (int i = 0; i < size; ++i) {
        ItemLocation loc{ItemLocation::Kind::Inventory, i};
        auto const&  s = itemAt(player, loc);
        if (!s.isNull() && pred(s)) return loc;
    }
    ItemLocation off{ItemLocation::Kind::Offhand, 0};
    if (auto const& s = itemAt(player, off); !s.isNull() && pred(s)) return off;
    return std::nullopt;
}

void consumeAt(Player& player, ItemLocation loc, int count, bool respectCreative) {
    if (respectCreative && player.isCreative()) return;
    ItemStack copy = itemAt(player, loc);
    if (copy.isNull()) return;
    int left = static_cast<int>(copy.mCount) - count;
    if (left <= 0) copy = ItemStack::EMPTY_ITEM();
    else copy.mCount = static_cast<uchar>(left);
    setItemAt(player, loc, copy);
}

void giveOrDrop(Player& player, ItemStack const& item) {
    if (item.isNull()) return;
    ItemStack copy = item;
    if (!player.add(copy)) player.drop(copy, false);
    player.sendInventory(false);
}

} // namespace bs::game
