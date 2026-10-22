// Warp modifiers (docs/SPEC.md §3.10, §4.6): the `modifiers` config, arrival effects and the
// server side of the Warp Modifiers screen.

#include "feature/modifiers/Modifiers.h"

#include <cmath>
#include <unordered_map>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/player/PlayerDisconnectEvent.h"
#include "ll/api/event/world/ServerLevelTickEvent.h"
#include "ll/api/form/SimpleForm.h"
#include "ll/api/service/Bedrock.h"

#include "mc/deps/core/math/Vec3.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Inventory.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/effect/EffectDuration.h"
#include "mc/world/effect/MobEffectInstance.h"
#include "mc/world/item/ItemStack.h"
#include "mc/world/level/Level.h"

#include "game/BlockContainer.h"
#include "game/Items.h"
#include "game/Waystones.h"
#include "mod/Config.h"
#include "mod/I18n.h"
#include "mod/Log.h"
#include "mod/Module.h"
#include "net/Screens.h"
#include "service/ScreenOpeners.h"
#include "service/Services.h"

BS_REGISTER_CONFIG("modifiers", bs::feature::modifiers::ModifiersConfig);

namespace bs::feature::modifiers {

namespace cm = core::modifiers;

namespace {

constexpr char const* kScreen = "modifiers";

// ---- conversions ------------------------------------------------------------------------------------

cm::Stack toCore(game::SlotItem const& s, int maxStack) {
    cm::Stack c;
    c.id       = s.id;
    c.count    = s.count;
    c.aux      = s.aux;
    c.user     = s.user;
    c.maxStack = maxStack;
    return c;
}

game::SlotItem toSlot(cm::Stack const& c) { return game::SlotItem{c.id, c.count, c.aux, c.user}; }

int maxStackOf(std::string const& id) {
    auto s = game::makeItem(id);
    return s.isNull() ? 64 : std::max<int>(1, s.getMaxStackSize());
}

cm::Slot fromStack(ItemStack const& stack) {
    auto s = game::toSlotItem(stack);
    if (!s) return std::nullopt;
    return toCore(*s, std::max<int>(1, stack.getMaxStackSize()));
}

nlohmann::json slotJson(cm::Slot const& s) {
    if (!s) return nullptr;
    return {{"id", s->id}, {"count", s->count}, {"aux", s->aux}};
}

cm::Slot slotFromJson(nlohmann::json const& j) {
    if (!j.is_object()) return std::nullopt;
    cm::Stack s;
    s.id    = j.value("id", std::string{});
    s.count = j.value("count", 0);
    s.aux   = j.value("aux", 0);
    if (auto it = j.find("user"); it != j.end() && it->is_object()) {
        for (auto const& [k, v] : it->items()) {
            if (v.is_string()) s.user[k] = v.get<std::string>();
        }
    }
    s.maxStack = j.value("maxStack", 64);
    if (s.id.empty() || s.count <= 0) return std::nullopt;
    return s;
}

std::string describe(cm::Slot const& s) { return s ? s->id + " x" + std::to_string(s->count) : "_"; }

// ---- distance -----------------------------------------------------------------------------------------

bool withinReach(Player& player, core::WaystoneRecord const& r, double max) {
    if (static_cast<int>(player.getDimensionId()) != r.pos.dim) return false;
    auto   feet = player.getFeetPos();
    double dx   = feet.x - (r.pos.x + 0.5);
    double dy   = feet.y - (r.pos.y + 0.5);
    double dz   = feet.z - (r.pos.z + 0.5);
    return dx * dx + dy * dy + dz * dz <= max * max;
}

// ---- the menu -------------------------------------------------------------------------------------------

void loadInventory(Player& player, cm::Menu& m);
void storeInventory(Player& player, cm::Menu const& before, cm::Menu const& after);

// ---- the cursor item -----------------------------------------------------------------------------------
// The item held on the cursor belongs to the player's open modifiers session (server side, mirrored
// in the model). When the session ends in any way (Esc, another screen, disconnect) it goes back
// to the inventory: the close action returns it, and a tick check catches sessions that ended
// without one.

struct Held {
    std::uint32_t session = 0;
    cm::Stack     item;
};
std::unordered_map<std::int64_t, Held> gHeld; // player unique id -> cursor

std::int64_t keyOf(Player const& p) { return p.getOrCreateUniqueID().rawID; }

cm::Slot heldCursor(Player const& p, std::uint32_t session) {
    auto it = gHeld.find(keyOf(p));
    if (it == gHeld.end() || it->second.session != session) return std::nullopt;
    return it->second.item;
}

void setHeld(Player const& p, std::uint32_t session, cm::Slot const& item) {
    if (item) gHeld[keyOf(p)] = Held{session, *item};
    else gHeld.erase(keyOf(p));
}

/// Gives the player's cursor item back to the inventory (dropping what does not fit).
void releaseCursor(Player& player) {
    auto it = gHeld.find(keyOf(player));
    if (it == gHeld.end()) return;
    cm::Menu before;
    before.cursor = it->second.item;
    gHeld.erase(it);
    loadInventory(player, before);
    auto after    = before;
    auto leftover = cm::returnCursor(after);
    storeInventory(player, before, after);
    if (leftover) player.drop(game::toStack(toSlot(*leftover)), false);
    log().info("modifiers: cursor item returned to {}", player.getRealName());
}


/// The menu as the server sees it: container + live player inventory + the session's cursor.
cm::Menu loadMenu(Player& player, core::WaystoneRecord const& r, std::uint32_t session) {
    cm::Menu m;
    auto     c = game::containerOf(r);
    for (std::size_t i = 0; i < c.size(); ++i) {
        if (c[i]) m.container[i] = toCore(*c[i], maxStackOf(c[i]->id));
    }
    loadInventory(player, m);
    m.cursor = heldCursor(player, session);
    return m;
}

/// The player inventory part of the menu.
void loadInventory(Player& player, cm::Menu& m) {
    auto& inv = player.getInventory();
    for (int i = 0; i < cm::kInventorySlots && i < inv.getContainerSize(); ++i) m.inventory[static_cast<std::size_t>(i)] = fromStack(inv.getItem(i));
}

/// Writes the changed inventory slots of `after` back and syncs the inventory.
void storeInventory(Player& player, cm::Menu const& before, cm::Menu const& after) {
    auto& inv     = player.getInventory();
    bool  changed = false;
    // Original stacks keep their full NBT when an equal item lands in another inventory slot.
    std::vector<ItemStack> originals;
    for (int i = 0; i < cm::kInventorySlots && i < inv.getContainerSize(); ++i) originals.push_back(inv.getItem(i));
    for (std::size_t i = 0; i < after.inventory.size() && i < originals.size(); ++i) {
        if (before.inventory[i] == after.inventory[i]) continue;
        changed = true;
        auto const& s = after.inventory[i];
        if (!s) {
            inv.setItem(static_cast<int>(i), ItemStack::EMPTY_ITEM());
            continue;
        }
        std::optional<ItemStack> stack;
        for (std::size_t j = 0; j < originals.size(); ++j) {
            if (before.inventory[j] && before.inventory[j]->sameItem(*s)) {
                stack = originals[j];
                break;
            }
        }
        if (!stack) stack = game::toStack(toSlot(*s));
        stack->set(s->count);
        inv.setItem(static_cast<int>(i), *stack);
    }
    if (changed) player.sendInventory(false);
}

/// Writes the changed parts of `after` back (container record, inventory slots, cursor).
void storeMenu(Player& player, core::WaystoneRecord& r, cm::Menu const& before, cm::Menu const& after, std::uint32_t session) {
    if (before.container != after.container) {
        game::Container c{};
        for (std::size_t i = 0; i < c.size(); ++i) {
            if (after.container[i]) c[i] = toSlot(*after.container[i]);
        }
        game::setContainer(r, c);
        game::Waystones::get().markDirty(r.uid);
    }
    storeInventory(player, before, after);
    setHeld(player, session, after.cursor);
}

nlohmann::json buildModel(Player& player, core::WaystoneRecord const& r, std::uint32_t session) {
    auto           m = loadMenu(player, r, session);
    nlohmann::json container = nlohmann::json::array(), inventory = nlohmann::json::array();
    for (auto const& s : m.container) container.push_back(slotJson(s));
    for (auto const& s : m.inventory) inventory.push_back(slotJson(s));
    auto ident = cm::galacticIdentifier(r.uid);
    return {
        {"uid", r.uid},
        {"type", r.type},
        {"galactic", ident},
        {"galacticColor", std::string(cm::galacticColor(ident))},
        {"container", container},
        {"inventory", inventory},
        {"cursor", slotJson(m.cursor)},
    };
}

core::WaystoneRecord* sessionRecord(Player& player, net::screens::ScreenSession& s) {
    auto* r = game::Waystones::get().find(s.context.value("uid", std::string{}));
    if (!r || !withinReach(player, *r, kMaxMenuDistance)) return nullptr;
    return r;
}

void handleAction(Player& player, net::screens::ScreenSession& s, std::string const& action, nlohmann::json const& args) {
    if (action == "close") {
        releaseCursor(player);
        return;
    }
    auto* r = sessionRecord(player, s);
    if (!r) {
        releaseCursor(player);
        net::screens::close(player);
        log().info("modifiers: {} is too far away or the waystone is gone; menu closed", player.getRealName());
        return;
    }
    auto before = loadMenu(player, *r, s.session);
    auto after  = before;
    int  slot   = args.value("slot", -1);
    bool done   = false;
    std::vector<cm::Stack> dropped;
    if (action == "click") {
        done = cm::click(after, slot, args.value("button", 0), &dropped);
    } else if (action == "quick_move") {
        done = cm::quickMove(after, slot);
    } else if (action == "hand_slot") {
        // Form fallback: the selected hotbar item acts as the cursor.
        auto hand    = game::handSlot(player);
        int  handId  = cm::kInventoryBase + hand.slot;
        auto* hs     = cm::slotOf(after, handId);
        if (hs && slot >= 0 && slot < cm::kContainerSlots) {
            after.cursor = *hs;
            hs->reset();
            done = cm::click(after, slot, 0);
            *hs  = after.cursor;
            after.cursor = before.cursor;
        }
    } else {
        return;
    }
    if (!done) {
        net::screens::update(player, buildModel(player, *r, s.session));
        return;
    }
    storeMenu(player, *r, before, after, s.session);
    for (auto const& d : dropped) player.drop(game::toStack(toSlot(d)), false);
    std::string contents;
    for (auto const& c : after.container) contents += (contents.empty() ? "" : ", ") + describe(c);
    log().info("modifiers: {} slot {} by {} on {} -> [{}]", action, slot, player.getRealName(), r->name, contents);
    net::screens::update(player, buildModel(player, *r, s.session));
}

// ---- form fallback --------------------------------------------------------------------------------------

std::string itemLabel(cm::Slot const& s) {
    if (!s) return "-";
    auto        stack = game::makeItem(s->id, s->count, s->aux);
    std::string name  = stack.isNull() ? s->id : stack.getName();
    if (name.empty()) name = s->id;
    return s->count > 1 ? name + " x" + std::to_string(s->count) : name;
}

void showFallback(Player& player, net::screens::ScreenSession& s) {
    auto const& m = s.model;
    ll::form::SimpleForm form(trFor(player, "container.waystones.warp_modifiers", "Warp Modifiers"));
    form.setContent(trFor(player, "bs.modifiers.form.hint", "Select a slot to put the held item into it, or to take its item."));
    auto container = m.value("container", nlohmann::json::array());
    for (int i = 0; i < cm::kContainerSlots; ++i) {
        auto slot  = i < static_cast<int>(container.size()) ? slotFromJson(container[static_cast<std::size_t>(i)]) : std::nullopt;
        auto label = i == 0 ? trFor(player, "bs.modifiers.form.shard_slot", "Shard") : trFor(player, "bs.modifiers.form.modifier_slot", "Modifier {}", i);
        form.appendButton(label + ": " + itemLabel(slot));
    }
    form.appendButton(trFor(player, "bs.modifiers.form.done", "Done"));
    form.sendTo(player, [](Player& p, int index, ll::form::FormCancelReason) {
        if (index >= 0 && index < cm::kContainerSlots) {
            net::screens::simulateAction(p, "hand_slot", {{"slot", index}});
        } else {
            net::screens::simulateAction(p, "close");
        }
    });
}

// ---- arrival effects ----------------------------------------------------------------------------------

class ModifierArrival final : public service::IArrival {
public:
    void onArrived(Actor& entity, core::WaystoneRecord const& target) override {
        if (!enabled()) return;
        auto fx = cm::arrivalEffects(modifierItems(target));
        if (fx.empty()) return;
        if (fx.cure) entity.removeAllEffects();
        if (fx.fireSeconds > 0) entity.setOnFire(fx.fireSeconds, true);
        for (auto const& e : fx.effects) {
            uint id = 0;
            switch (e.effect) {
            case cm::Effect::Poison: id = 19; break;
            case cm::Effect::Blindness: id = 15; break;
            case cm::Effect::SlowFalling: id = 27; break;
            case cm::Effect::FireResistance: id = 12; break;
            case cm::Effect::Wither: id = 20; break;
            }
            EffectDuration duration{};
            duration.mValue = e.seconds * 20;
            MobEffectInstance inst(id, duration);
            inst.mAmplifier = e.amplifier;
            entity.addEffect(inst);
        }
        log().info("modifiers: arrival effects applied at {} (cure {}, fire {}s, {} effects)", target.name, fx.cure, fx.fireSeconds, fx.effects.size());
    }
};

class ModifiersModule {
public:
    void install() {
        service::set<service::IArrival>(std::make_unique<ModifierArrival>());
        auto& bus = ll::event::EventBus::getInstance();
        // Sessions that ended without a close action (replaced by another screen, dropped).
        mListeners.push_back(bus.emplaceListener<ll::event::ServerLevelTickEvent>([](auto&) {
            if (gHeld.empty()) return;
            auto level = ll::service::getLevel();
            if (!level) return;
            std::vector<std::int64_t> ended;
            for (auto const& [id, held] : gHeld) {
                auto* p = level->getPlayer(ActorUniqueID{id});
                if (!p || !net::screens::current(*p, kScreen, held.session)) ended.push_back(id);
            }
            for (auto id : ended) {
                if (auto* p = level->getPlayer(ActorUniqueID{id})) releaseCursor(*p);
                else gHeld.erase(id);
            }
        }));
        mListeners.push_back(bus.emplaceListener<ll::event::PlayerDisconnectEvent>([](ll::event::PlayerDisconnectEvent& ev) {
            releaseCursor(ev.self());
        }));
    }
    void uninstall() {
        auto& bus = ll::event::EventBus::getInstance();
        for (auto& l : mListeners) bus.removeListener(l);
        mListeners.clear();
        gHeld.clear();
        service::reset<service::IArrival>();
    }

private:
    std::vector<ll::event::ListenerPtr> mListeners;
};

} // namespace

bool enabled() { return config::get<ModifiersConfig>().enableModifiers; }

std::vector<cm::Stack> modifierItems(core::WaystoneRecord const& record) {
    std::vector<cm::Stack> out;
    auto                   c = game::containerOf(record);
    for (std::size_t i = 1; i < c.size(); ++i) {
        if (c[i]) out.push_back(toCore(*c[i], 64));
    }
    return out;
}

int activeModifierCount(core::WaystoneRecord const& record) { return cm::activeModifierCount(modifierItems(record)); }

bool openModifiers(Player& player, std::string const& uid) {
    auto* r = game::Waystones::get().find(uid);
    if (!r) return false;
    if (!withinReach(player, *r, kMaxMenuDistance)) {
        log().info("modifiers: {} is too far away from {}", player.getRealName(), r->name);
        return false;
    }
    releaseCursor(player); // left over from an earlier session
    nlohmann::json context{{"uid", uid}};
    auto           model = buildModel(player, *r, 0);
    if (!net::screens::open(player, kScreen, std::move(model), std::move(context))) return false;
    log().info("modifiers: menu of {} opened by {}", r->name, player.getRealName());
    return true;
}

} // namespace bs::feature::modifiers

BS_REGISTER_MODULE(bs::feature::modifiers::ModifiersModule);

BS_REGISTER_SCREEN_OPENER("modifiers", [](Player& player, nlohmann::json const& args) {
    bs::feature::modifiers::openModifiers(player, args.value("uid", std::string{}));
});

BS_REGISTER_SCREEN_ACTIONS("modifiers", bs::feature::modifiers::handleAction);
BS_REGISTER_SCREEN_FALLBACK("modifiers", bs::feature::modifiers::showFallback);
