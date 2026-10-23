#include "game/BlockContainer.h"

#include "mc/deps/nbt/CompoundTag.h"
#include "mc/world/item/ItemStack.h"

#include "game/Ids.h"
#include "game/Items.h"

namespace bs::game {

namespace {
/// User-data keys carried through the container (all of Bedstones' item NBT).
constexpr std::string_view kUserKeys[] = {
    ids::kNbtWaystone,
    ids::kNbtAttunement,
    ids::kNbtBoundUid,
    ids::kNbtBoundName,
    ids::kNbtFeather,
    ids::kNbtFeatherLink,
};

std::optional<SlotItem> slotFromJson(nlohmann::json const& j) {
    if (!j.is_object()) return std::nullopt;
    SlotItem s;
    s.id    = j.value("id", std::string{});
    s.count = j.value("count", 1);
    s.aux   = j.value("aux", 0);
    if (auto it = j.find("user"); it != j.end() && it->is_object()) {
        for (auto const& [k, v] : it->items()) {
            if (v.is_string()) s.user[k] = v.get<std::string>();
        }
    }
    s.nbt = j.value("nbt", std::string{});
    if (s.id.empty() || s.count <= 0) return std::nullopt;
    return s;
}

nlohmann::json slotToJson(std::optional<SlotItem> const& s) {
    if (!s) return nullptr;
    nlohmann::json j{{"id", s->id}, {"count", s->count}, {"aux", s->aux}, {"user", s->user}};
    if (!s->nbt.empty()) j["nbt"] = s->nbt;
    return j;
}
} // namespace

Container containerOf(core::WaystoneRecord const& record) {
    Container c{};
    auto      it = record.extra.find("container");
    if (it == record.extra.end() || !it->is_array()) return c;
    for (std::size_t i = 0; i < kContainerSlots && i < it->size(); ++i) c[i] = slotFromJson((*it)[i]);
    return c;
}

void setContainer(core::WaystoneRecord& record, Container const& container) {
    nlohmann::json arr = nlohmann::json::array();
    for (auto const& s : container) arr.push_back(slotToJson(s));
    record.extra["container"] = std::move(arr);
}

std::optional<SlotItem> toSlotItem(ItemStack const& stack) {
    if (stack.isNull() || stack.mCount == 0) return std::nullopt;
    SlotItem s;
    s.id    = idOf(stack);
    s.count = stack.mCount;
    s.aux   = stack.getAuxValue();
    for (auto key : kUserKeys) {
        if (auto v = userString(stack, key)) s.user[std::string(key)] = *v;
    }
    if (stack.mUserData && !stack.mUserData->empty()) s.nbt = stack.mUserData->toSnbt(SnbtFormat::Minimize, 0);
    return s;
}

ItemStack toStack(SlotItem const& item) {
    auto stack = makeItem(item.id, item.count, item.aux);
    if (!item.nbt.empty() && !stack.isNull()) {
        if (auto tag = CompoundTag::fromSnbt(item.nbt)) {
            stack.setUserData(std::make_unique<CompoundTag>(std::move(*tag)));
            return stack;
        }
    }
    for (auto const& [k, v] : item.user) setUserString(stack, k, v);
    return stack;
}

} // namespace bs::game
