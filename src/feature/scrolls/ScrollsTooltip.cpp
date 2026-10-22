#ifdef BS_CLIENT

// Client mod: Waystones' static item tooltip lines that Bedrock items cannot carry in their JSON
// (the Blank Scroll's "Use this scroll on a waystone to bind it."). Bound scrolls show their
// "Bound to" line through item lore set by the server, so vanilla clients see it too.

#include "ll/api/memory/Hook.h"

#include "mc/safety/RedactableString.h"
#include "mc/world/item/ItemStackBase.h"
#include "mc/world/item/components/ComponentItem.h"
#include "mc/world/level/Level.h"

#include "client/Client.h"
#include "game/Ids.h"
#include "mod/Module.h"

namespace bs::feature::scrolls {

namespace {

LL_TYPE_INSTANCE_HOOK(
    ScrollsHovertextHook,
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
    static std::string const blank = ids::id(ids::kBlankScroll);
    if (stack.getTypeName() == blank) {
        hovertext += "\n§7" + client::ctr("tooltip.waystones.blank_scroll", "Use this scroll on a waystone to bind it.");
    }
}

class ScrollsTooltipModule {
public:
    void install() { ScrollsHovertextHook::hook(); }
    void uninstall() { ScrollsHovertextHook::unhook(); }
};

} // namespace

BS_REGISTER_MODULE(ScrollsTooltipModule);

} // namespace bs::feature::scrolls

#endif
