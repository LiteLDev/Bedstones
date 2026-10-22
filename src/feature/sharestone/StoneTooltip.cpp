#ifdef BS_CLIENT

// Item tooltips of sharestones ("Teleport to any other Red Sharestone") and portstones ("Can not be
// teleported to."), gray like Java's appendHoverText. Client side only: the hover text is built by
// the client from the item, so this needs no item data and works for every stack.

#include <string>

#include "ll/api/memory/Hook.h"

#include "mc/locale/I18n.h"
#include "mc/locale/Localization.h"
#include "mc/safety/RedactableString.h"
#include "mc/world/item/Item.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/item/components/ComponentItem.h"
#include "mc/world/level/Level.h"

#include "client/Client.h"
#include "feature/sharestone/StoneBlocks.h"
#include "game/Ids.h"
#include "mod/Module.h"

namespace bs::feature::sharestone {

namespace {

/// The block's own name in the game's language (not the stack's: it may be renamed).
std::string blockName(StoneKind const& kind) {
    std::string key  = "tile." + ids::id(core::sharestone::path(kind)) + ".name";
    auto&       i18n = getI18n();
    auto        name = i18n.get(key, i18n.getCurrentLanguage());
    return name.empty() || name == key ? core::sharestone::displayName(kind) : name;
}

void appendStoneLine(::ItemStackBase const& stack, ::Level& level, ::Bedrock::Safety::RedactableString& hovertext) {
    if (!level.isClientSide() || stack.isNull()) return;
    auto kind = stoneKindOf(stack.getTypeName());
    if (!kind) return;
    std::string line = kind->family == Family::Sharestone
                         ? client::ctr("bs.sharestone.tooltip", "Teleport to any other {}", blockName(*kind))
                         : client::ctr("tooltip.waystones.portstone", "Can not be teleported to.");
    line             = "§7" + line;
    auto& s          = hovertext.mUnredactedString;
    if (s.find(line) != std::string::npos) return; // both hooks ran (ComponentItem calls Item)
    if (!s.empty() && s.back() != '\n') s += '\n';
    s += line;
}

} // namespace

LL_TYPE_INSTANCE_HOOK(
    BsStoneHoverTextHook,
    ll::memory::HookPriority::Normal,
    Item,
    &Item::$appendFormattedHovertext,
    void,
    ::ItemStackBase const&               stack,
    ::Level&                             level,
    ::Bedrock::Safety::RedactableString& hovertext,
    bool                                 showCategory
) {
    origin(stack, level, hovertext, showCategory);
    appendStoneLine(stack, level, hovertext);
}

LL_TYPE_INSTANCE_HOOK(
    BsStoneComponentHoverTextHook,
    ll::memory::HookPriority::Normal,
    ComponentItem,
    &ComponentItem::$appendFormattedHovertext,
    void,
    ::ItemStackBase const&               stack,
    ::Level&                             level,
    ::Bedrock::Safety::RedactableString& hovertext,
    bool const                           showCategory
) {
    origin(stack, level, hovertext, showCategory);
    appendStoneLine(stack, level, hovertext);
}

class StoneTooltip {
public:
    void install() {
        BsStoneHoverTextHook::hook();
        BsStoneComponentHoverTextHook::hook();
    }
    void uninstall() {
        BsStoneComponentHoverTextHook::unhook();
        BsStoneHoverTextHook::unhook();
    }
};

} // namespace bs::feature::sharestone

BS_REGISTER_MODULE(bs::feature::sharestone::StoneTooltip);

#endif // BS_CLIENT
