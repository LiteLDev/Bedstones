#include "core/modifiers/Modifiers.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace bs::core::modifiers {

bool isWarpShard(std::string_view id) {
    return id == "bedstones:dormant_shard" || id == "bedstones:attuned_shard" || id == "bedstones:crumbling_attuned_shard"
        || id == "bedstones:deepslate_shard";
}

// ---- effects ----------------------------------------------------------------------------------------

namespace {
int countOf(std::vector<Stack> const& items, std::string_view id) {
    int n = 0;
    for (auto const& s : items) {
        if (s.id == id && s.count > 0) n += s.count;
    }
    return n;
}
} // namespace

int potency(std::vector<Stack> const& modifiers) {
    return std::min(kMaxPotency, 1 + countOf(modifiers, kDiamond));
}

ArrivalEffects arrivalEffects(std::vector<Stack> const& modifiers) {
    ArrivalEffects out;
    int            amp = potency(modifiers);
    out.cure           = countOf(modifiers, "minecraft:milk_bucket") > 0 || countOf(modifiers, "minecraft:honey_block") > 0;
    out.fireSeconds    = countOf(modifiers, "minecraft:blaze_powder");
    auto add           = [&](Effect e, int seconds) {
        if (seconds > 0) out.effects.push_back({e, seconds, amp});
    };
    add(Effect::Poison, countOf(modifiers, "minecraft:poisonous_potato"));
    add(Effect::Blindness, countOf(modifiers, "minecraft:ink_sac"));
    add(Effect::SlowFalling, std::min(kMaxShortEffectSeconds, countOf(modifiers, "minecraft:feather")));
    add(Effect::FireResistance, std::min(kMaxShortEffectSeconds, countOf(modifiers, "minecraft:magma_cream")));
    add(Effect::Wither, countOf(modifiers, "minecraft:wither_rose"));
    return out;
}

int activeModifierCount(std::vector<Stack> const& modifiers) {
    int n = 0;
    for (auto const& s : modifiers) {
        if (s.id.empty() || s.count <= 0 || s.id == "minecraft:amethyst_shard" || s.id == "minecraft:slime_ball") continue;
        n += s.count;
    }
    return n;
}

// ---- galactic name ------------------------------------------------------------------------------------

std::int32_t javaHash(std::string_view s) {
    std::uint32_t h = 0;
    for (unsigned char c : s) h = 31u * h + c; // UTF-16 code units == bytes for ASCII uids
    return static_cast<std::int32_t>(h);
}

std::string galacticIdentifier(std::string_view uid) {
    std::string out;
    for (char c : uid) {
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) continue;
        out.push_back(c);
        if (out.size() == 8) break;
    }
    return out;
}

std::string_view galacticColor(std::string_view identifier) {
    static constexpr std::string_view kById[] = {
        "black", "dark_blue", "dark_green", "dark_aqua", "dark_red", "dark_purple", "gold", "gray",
        "dark_gray", "blue", "green", "aqua", "red", "light_purple", "yellow", "white",
    };
    auto h  = static_cast<std::int64_t>(javaHash(identifier));
    auto id = static_cast<std::size_t>(std::llabs(h) % 15);
    auto c  = kById[id];
    if (c == "gray") return "light_purple";
    if (c == "dark_gray") return "dark_purple";
    if (c == "black") return "gold";
    return c;
}

// ---- menu -----------------------------------------------------------------------------------------------

Slot* slotOf(Menu& menu, int slotId) {
    if (slotId >= 0 && slotId < kContainerSlots) return &menu.container[static_cast<std::size_t>(slotId)];
    int inv = slotId - kInventoryBase;
    if (inv >= 0 && inv < kInventorySlots) return &menu.inventory[static_cast<std::size_t>(inv)];
    return nullptr;
}

bool mayPlace(int slotId, Stack const& stack) { return slotId != 0 || isWarpShard(stack.id); }

int slotLimit(int slotId, Stack const& stack) { return slotId == 0 ? 1 : std::max(1, stack.maxStack); }

namespace {

/// Java `moveItemStackTo`: merges into equal stacks first, then into the first empty slot that
/// accepts it. Returns true when something moved.
bool moveTo(Menu& menu, Stack& stack, std::vector<int> const& targets) {
    bool moved = false;
    if (stack.maxStack > 1) {
        for (int id : targets) {
            if (stack.count <= 0) break;
            auto* slot = slotOf(menu, id);
            if (!slot || !*slot || !(*slot)->sameItem(stack)) continue;
            int room = slotLimit(id, stack) - (*slot)->count;
            if (room <= 0) continue;
            int n = std::min(room, stack.count);
            (*slot)->count += n;
            stack.count -= n;
            moved = true;
        }
    }
    if (stack.count > 0) {
        for (int id : targets) {
            auto* slot = slotOf(menu, id);
            if (!slot || *slot || !mayPlace(id, stack)) continue;
            Stack put = stack;
            put.count = std::min(stack.count, slotLimit(id, stack));
            stack.count -= put.count;
            *slot = put;
            moved = true;
            break;
        }
    }
    return moved;
}

/// Player inventory slots in Java's reverse menu order: hotbar 8..0, then inventory 35..9.
std::vector<int> inventoryTargets() {
    std::vector<int> out;
    for (int i = 8; i >= 0; --i) out.push_back(kInventoryBase + i);
    for (int i = 35; i >= 9; --i) out.push_back(kInventoryBase + i);
    return out;
}

bool quickMoveOnce(Menu& menu, int slotId) {
    auto* slot = slotOf(menu, slotId);
    if (!slot || !*slot) return false;
    Stack stack = **slot;
    bool  moved;
    if (slotId < kContainerSlots) {
        moved = moveTo(menu, stack, inventoryTargets());
    } else if (!menu.container[0] && mayPlace(0, stack)) {
        moved = moveTo(menu, stack, {0});
    } else {
        moved = moveTo(menu, stack, {1, 2, 3, 4});
    }
    if (!moved) return false;
    if (stack.count > 0) *slot = stack;
    else slot->reset();
    return true;
}

} // namespace

bool quickMove(Menu& menu, int slotId) {
    auto* slot = slotOf(menu, slotId);
    if (!slot || !*slot) return false;
    Stack original = **slot;
    bool  any      = false;
    // Java repeats the quick move while the slot still holds the same item and something moved.
    for (int guard = 0; guard < 64 && quickMoveOnce(menu, slotId); ++guard) {
        any = true;
        if (!*slot || !(*slot)->sameItem(original)) break;
    }
    return any;
}

Slot returnCursor(Menu& menu) {
    if (!menu.cursor) return std::nullopt;
    Stack stack = *menu.cursor;
    menu.cursor.reset();
    for (auto& slot : menu.inventory) {
        if (stack.count <= 0) break;
        if (!slot || !slot->sameItem(stack)) continue;
        int n = std::min(stack.count, std::max(1, stack.maxStack) - slot->count);
        if (n <= 0) continue;
        slot->count += n;
        stack.count -= n;
    }
    for (auto& slot : menu.inventory) {
        if (stack.count <= 0) break;
        if (slot) continue;
        Stack put = stack;
        put.count = std::min(stack.count, std::max(1, stack.maxStack));
        stack.count -= put.count;
        slot = put;
    }
    if (stack.count <= 0) return std::nullopt;
    return stack;
}

bool click(Menu& menu, int slotId, int button, std::vector<Stack>* dropped) {
    if (button != 0 && button != 1) return false;
    auto& cursor = menu.cursor;
    if (slotId == kOutside) {
        if (!cursor) return false;
        Stack d = *cursor;
        if (button == 1) {
            d.count = 1;
            if (--cursor->count <= 0) cursor.reset();
        } else {
            cursor.reset();
        }
        if (dropped) dropped->push_back(d);
        return true;
    }
    auto* slot = slotOf(menu, slotId);
    if (!slot) return false;
    if (!cursor) {
        if (!*slot) return false;
        // Pick up: everything (left) or the larger half (right).
        Stack& s    = **slot;
        int    take = button == 0 ? s.count : (s.count + 1) / 2;
        Stack  held = s;
        held.count  = take;
        s.count -= take;
        if (s.count <= 0) slot->reset();
        cursor = held;
        return true;
    }
    Stack& held = *cursor;
    if (!*slot) {
        if (!mayPlace(slotId, held)) return false;
        int   n   = std::min(button == 0 ? held.count : 1, slotLimit(slotId, held));
        Stack put = held;
        put.count = n;
        *slot     = put;
        held.count -= n;
        if (held.count <= 0) cursor.reset();
        return true;
    }
    Stack& s = **slot;
    if (s.sameItem(held)) {
        if (!mayPlace(slotId, held)) return false;
        int room = slotLimit(slotId, held) - s.count;
        int n    = std::min(button == 0 ? held.count : 1, room);
        if (n <= 0) return false;
        s.count += n;
        held.count -= n;
        if (held.count <= 0) cursor.reset();
        return true;
    }
    // Different items: swap when the held stack fits.
    if (!mayPlace(slotId, held) || held.count > slotLimit(slotId, held)) return false;
    std::swap(*slot, cursor);
    return true;
}

} // namespace bs::core::modifiers
