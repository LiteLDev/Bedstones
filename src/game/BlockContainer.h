#pragma once

// The 5-slot container of waystones and warp plates (Waystones' modifier inventory), stored in the
// record's `extra["container"]` because custom blocks have no block entities (server thread).
//
//   slot 0     shard slot (warp plates: dormant / attuned / crumbling / deepslate shard, stack 1)
//   slots 1-4  modifier items (any item, Waystones' warp modifiers have effects)
//
// JSON: "container": [ null | {"id": "minecraft:diamond", "count": 2, "aux": 0,
//                              "user": {"bs_attunement": "<uid>", ...}}, ... ]  (always 5 entries)
// `user` holds the item's user-data strings (all of Bedstones' item NBT is strings, game/Ids.h).
//
// Writers: U3 (shard insert / eject / attunement), U13 (Warp Modifiers screen). Readers: U3 (target
// choice, plate timing, redstone), U7/U13 (arrival effects), U1/U3 (drop contents on break).

#include <array>
#include <map>
#include <optional>
#include <string>

#include "core/Waystone.h"

class ItemStack;

namespace bs::game {

struct SlotItem {
    std::string                        id;
    int                                count = 1;
    int                                aux   = 0;
    std::map<std::string, std::string> user; ///< user-data strings (NBT keys from ids::kNbt*)

    [[nodiscard]] bool operator==(SlotItem const&) const = default;
};

inline constexpr std::size_t kContainerSlots = 5;
using Container                              = std::array<std::optional<SlotItem>, kContainerSlots>;

/// The container of a record (empty slots when it has none).
[[nodiscard]] Container containerOf(core::WaystoneRecord const& record);
/// Stores the container in the record. Call `Waystones::markDirty(uid)` afterwards.
void setContainer(core::WaystoneRecord& record, Container const& container);

/// ItemStack <-> slot item (only id, count, aux and user-data strings survive).
[[nodiscard]] std::optional<SlotItem> toSlotItem(ItemStack const& stack);
[[nodiscard]] ItemStack               toStack(SlotItem const& item);

} // namespace bs::game
