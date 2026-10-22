<!-- Behaviour reference. Bedstones mapping notes:
  - ids are `bedstones:<path>` with Waystones' paths (game/Ids.h); integer block states `bedstones:facing` (Java 2D value), `bedstones:upper`, `bedstones:status`.
  - Java concepts without a Bedrock equivalent (map mods, stats) are out of scope; see AGENTS.md.
-->
# Waystones mod (BlayTheNinth): feature specification

Behaviour reference for Bedstones, written from reading Waystones' public source (`TwelveIterationMods/Waystones`). Bedstones reproduces the **behaviour**; no code or assets are copied.

**Which version this spec describes**
- **Primary reference: branch `1.21.1`, mod version 21.1.46.** This is the most feature-complete 1.21.x build. It has groups, the Twinbound Feather, the Epitaph and the Portal Scroll, backported from the 26.x line.
- **Also checked:**
  - Branch `1.21.11`, mod 21.11.9. It has an older "classic" feature set and a paged UI with up/down arrows.
  - Branch `26.3`, mod 26.3.0.1, the latest. It is a large redesign that needs the Shogi rules library.
- Version-specific details are marked **[21.1.46]**, **[21.11]** or **[26.x]**.
- Mod id is `waystones`. All ids below are `waystones:` unless they say otherwise.

---

## 1. Blocks

### 1.1 Shared block properties (`WaystoneBlockBase`)
- **Strength:** hardness 5.0, blast resistance 2000. Needs the correct tool (tag `minecraft:mineable/pickaxe`).
- **Sound:** STONE. Exceptions: deepslate uses DEEPSLATE, red nether bricks uses NETHER_BRICKS, mud bricks uses MUD_BRICKS.
- **Pistons:** push reaction is BLOCK, so pistons cannot move these blocks. They are also in the tag `c:relocation_not_supported`.
- **Block states:**
  - `facing`: horizontal. On placement it is set to the opposite of the player's facing.
  - `waterlogged`: boolean.
  - `origin`: one of `unknown`, `wilderness`, `dungeon`, `village`, `player`.
  - `half`: `lower`/`upper`, for the 2-tall blocks.
- **Placement:**
  - Needs a replaceable block above and `y < height - 1`.
  - Places the upper half automatically, and waterlogs it if it was placed into water.
  - Neither half can survive without the other. Breaking either half removes both.
- **When placed by a player:**
  - Creates the waystone record (new UUID, owner = placer UUID and name, origin PLAYER).
  - Activates it for the placer.
  - Opens the Edit screen automatically. Warp plates and fleeting memorials skip this step.
  - If the placed item carries the `waystones:waystone` UUID component (from a silk-touched pickup), the existing record is restored instead of a new one being created. Name, owner, activations and visibility are kept, and the record is marked non-transient again.
- **Breaking:**
  - Survival breaking with the correct tool drops one item, from the lower half only (the loot table has a `half=lower` condition and `survives_explosion`).
  - Items in the modifier container are dropped too. Creative breaking drops nothing.
  - **No silk touch:** the record is deleted, every player "forgets" it, and a removal message is sent to everyone.
  - **Silk touch** (waystones and warp plates only; `canSilkTouch` is true): the drop carries the UUID (loot function `copy_components` from the block entity). The record stays but is marked `Transient`, which hides it from global lists and map mods.
  - Sharestones, portstones and fleeting memorials do not preserve the record with silk touch.
- **No owner-based break protection in 21.1.46.** Anyone with a pickaxe can break any waystone. The lang key `chat.waystones.cannot_break_waystone` ("You cannot break this waystone.") is a leftover from older versions.
- **Observers:** on activation, adjacent observers that face the block get a scheduled tick (a redstone pulse).
- **Interaction priority:**
  1. **Shift + right-click** opens the Edit/Settings menu (`handleEditActions`).
  2. Otherwise the block's activation behaviour runs.
- **Blank Scroll:** right-clicking with a blank scroll skips the block interaction so the item's own use-on handles it.
- **Item tooltip:** a placed-then-picked-up item shows the waystone name in AQUA.
- **Hidden debug feature (creative only):**
  - Right-click with a **bamboo** resets the waystone ("Waystone was successfully reset - it will re-initialize once it is next loaded.").
  - Right-click with a **stick** prints the server and client UUIDs.

### 1.2 Waystone variants (2 tall)
All variants share the behaviour of `WaystoneBlock`. The waystone type is `waystones:waystone` for every variant; the variant only changes the "style".

| Block id | Display name (en_us) | Recipe material "S" | Rune colour |
|---|---|---|---|
| `waystone` | Waystone | stone_bricks | white |
| `mossy_waystone` | Mossy Waystone | mossy_stone_bricks, or shapeless: waystone + 3 moss_block, or waystone + 3 vine | white |
| `sandy_waystone` | Sandy Waystone | chiseled_sandstone | white |
| `deepslate_waystone` | Deepslate Waystone | deepslate | white |
| `blackstone_waystone` | **Black Waystone** | blackstone | `0x993333` |
| `end_stone_waystone` | **Ender Waystone** | end_stone_bricks | `0x7200FF` |
| `red_nether_bricks_waystone` | **Nether Waystone** | red_nether_bricks | white |
| `purpur_waystone` | Purpur Waystone | purpur_block | white |
| `prismarine_waystone` | Prismarine Waystone | prismarine | white |
| `mud_bricks_waystone` | **Muddy Waystone** | mud_bricks | white |

- **Recipe (shaped):**
  ```
   S
  SWS
  OOO
  ```
  S = material, W = `warp_stone`, O = obsidian.
- **Shapes:**
  - Lower half: 16×3×16 base, then 14×4 at y3–7, 12×2 at y7–9, 10×7 at y9–16.
  - Upper half: 10×8, then 12×2, then 14×2 (y10–12), 10×2, 8×2 cap.
- **Not pathfindable.**
- **Models:** `<style>_waystone_bottom` / `_top`, rotated by `y` per facing (north 0, east 90, south 180, west 270).
- **Textures:**
  - One atlas-style texture per style: `andesite_waystone.png` (the default waystone), `mossy_andesite_waystone.png`, `chiseled_sandstone_waystone.png`, and similar.
  - Particle textures are vanilla blocks, e.g. `polished_andesite`.
- **Runes overlay:**
  - Model `waystone_runes` with texture `waystone_runes.png`, drawn by a block entity renderer.
  - Drawn **only if the viewing player has activated the waystone**.
  - Uses cutout rendering, tinted with the style's rune colour.
  - Full-bright (light 15728880) unless the client option `disableTextGlow` is on.
- **Ambient particles** (client, `animateTick`): with 75% chance per tick, if the viewer has activated it, spawn 1 PORTAL and 1 ENCHANT particle at y+0.5 within ±0.75 blocks.

### 1.3 Sharestones: 15 dyed variants **[21.1.46]**
- **Ids:** `{orange, magenta, light_blue, yellow, lime, pink, gray, light_gray, cyan, purple, blue, brown, green, red, black}_sharestone`. There is no white sharestone.
- **Waystone type** equals the block id, e.g. `waystones:red_sharestone`. Visibility is the matching `RED_SHARESTONE` etc.
- **Recipe:**
  ```
  SSS
  DWD
  OOO
  ```
  S = stone_bricks, D = `c:dyes/<color>`, W = warp_stone, O = obsidian.
- **Shape:** the lower half matches the waystone. The upper half widens to 16×3 at the top (y13–16).
- **Right-click:** opens the selection list of **all sharestones of the same colour on the server**. No activation is needed. Shift + right-click opens the Edit screen.
- **Tooltip:** "Teleport to any other Red Sharestone" (gray).
- **Renderer:**
  - Sharestone runes (`sharestone_runes.png`) tinted with the dye's diffuse colour, always shown.
  - A floating **warp stone item** (with an Unbreaking glint) spins above at y+1. Angle = `(gameTime % 720 + partial) / 2` degrees; bob = `sin(t/8) × 0.025`; scale 0.5.
- **On initialization** an update is broadcast to all players, because everyone can see sharestones.

### 1.4 Portstones: 16 colours including white
- **Ids:** `white_portstone` (name "Portstone"), plus `<color>_portstone` for the 15 colours.
- **Recipe:**
  ```
  DSD
  SWS
  BBB
  ```
  D = dye (white dye for white), S = stone_bricks, W = warp_stone, B = polished_andesite.
- **No waystone record of its own.** `PortstoneBlockEntity` is only for rendering. Tooltip: "Can not be teleported to."
- **Right-click (shift or not):** opens a selection list with the flag `PORTSTONE`. Title `container.waystones.<color>_portstone`.
  - White: the player's activated waystones plus global and team waystones.
  - Coloured: all sharestones of that colour.
- **Shape:** a 2-tall pedestal with a lectern-like slanted top that differs per facing (4 shape sets).
- **Renderer:** portstone runes (`portstone_runes.png`) in dye colour.

### 1.5 Warp Plate (1 tall)
- **Recipe:**
  ```
  SWS
  WFW
  SWS
  ```
  S = stone_bricks, W = warp_dust, F = dormant_shard.
- **Shape:** 16×1×16 base plus a 10×1×10 raised centre.
- **Block state** `status`: `empty | idle | attuning | warping | warping_invalid | redstone_disabled | locked`. Models: `warp_plate`, `warp_plate_empty`, `warp_plate_locked`. Textures: `warp_plate`, `_base`, `_inner`, `_lock`, `_runes`.
- **Tint:** the block is coloured by a hash of its "galactic identifier".
- **Galactic name and identifier:**
  - Identifier: the UUID with digits and dashes removed, first 8 characters.
  - Displayed in the Standard Galactic Alphabet font (`minecraft:alt`).
  - Colour = `ChatFormatting.getById(|hash| % 15)`, with GRAY→LIGHT_PURPLE, DARK_GRAY→DARK_PURPLE, BLACK→GOLD.
- **Naming:** gets a random name on placement (RANDOM_ONLY name generator). It does **not** open a menu on placement.
- **Visibility:** always `SHARD_ONLY`.
- **Container:** 5 slots. Slot 0 holds the shard (stack size 1 for a dormant shard). Slots 1–4 hold modifiers.
- **Interactions:**
  - Right-click holding an item in `#waystones:warp_shards` while the shard slot is empty: inserts one shard (creative does not consume it).
  - Shift + right-click with an empty hand: ejects the shard as an item entity.
  - Right-click: opens the **Edit screen**, which leads to the Warp Modifiers container.
- **Attunement cycle (server tick):**
  - A newly placed plate contains a **dormant shard**, so its status is `attuning`.
  - After **30 ticks** the shard becomes an `attuned_shard` bound to this plate.
  - A shard bound to *this* plate is automatically **ejected** as an item, with a chicken-egg sound and a SMALL_GUST particle burst. The player then puts it into **another** plate, and that plate warps to this one.
  - A deepslate shard sets status `locked`: the plate cannot teleport, but it is still a target.
- **Teleport flow:**
  - An entity enters: not a passenger, not on the deny-list, and not on cooldown for this plate.
  - If the status is idle and a target exists, the status becomes `warping`, or `warping_invalid` if the target is invalid or the player can't afford it.
  - Every tick the entity stays on the plate, a counter increases.
  - When the counter exceeds `warpPlateUseTime` (default 15 ticks), the entity is teleported to the bound target.
  - If several shards are present, the target is chosen randomly. A **Quartz** modifier makes it round-robin, and a **Spider Eye** modifier prefers crumbling shards.
  - `crumbling_attuned_shard` (tag `single_use_warp_shards`) is consumed after use (not in creative).
- **Cooldown:** after arriving on a plate, the entity must step off for `warpPlateCooldownTime` (20 ticks) before that plate can send it back.
- **Action-bar messages** (dark red):
  - "This warp plate is not bound to another warp plate yet."
  - "The shard inside this warp plate has lost its attunement."
- **Redstone:** with a redstone torch modifier (tag `warp_modifiers/redstone_sensitive`), a powered plate is disabled (status `redstone_disabled`).
- **Use-time modifiers:** each amethyst shard −1.6%, each slime ball +1.6%. The result is clamped to `[1, 2 × configured]`.
- **Particles per status:**

  | Status | Particles |
  |---|---|
  | warping | 50 × (CRIMSON_SPORE + PORTAL) per tick, over a 2-block height |
  | warping_invalid | 10 SMOKE |
  | redstone_disabled | 1 redstone dust |
  | attuning | 10 WARPED_SPORE + 10 SOUL_FIRE_FLAME |

- **Arrival position:** the plate itself. There is no facing offset.

### 1.6 Warp Portal (no item; created by the Portal Scroll)
- **Shape:** 2-tall, `facing` + `half`. No collision or outline. Light level 10. Unbreakable (hardness −1, resistance 3,600,000). No loot.
- **Animated texture:** `warp_portal.png` with an `.mcmeta`.
- **Lifetime:** 6000 ticks (5 min). On expiry: fire-extinguish sound, 32 REVERSE_PORTAL particles, then the block is removed.
- **Behaviour:**
  - Owns a transient waystone record of type `warp_portal` named "Return to Portal".
  - An entity entering is teleported to the portal's target. There is a 20-tick per-entity re-use cooldown.
  - The entity's persistent data stores `ReturnPortal = <portal waystone uid>`.
  - If the target is gone: "This portal has closed because its target was destroyed." and the portal is removed.
- **Spawn position:** 3–4 blocks ahead along the player's look direction (horizontal component). It scans 5 blocks downward from y+2, looking for a solid block below and 3 replaceable blocks.
- **Spawn effects:** PORTAL_TRAVEL sound (volume 0.25, pitch 1.4), 96 PORTAL and 48 REVERSE_PORTAL particles.

### 1.7 Fleeting Memorial (no item; created by the Epitaph)
- **Block:** 1 tall, a thin gravestone-like slab with facing-dependent shapes. Hardness 2, resistance 6, light 6. No loot. Render shape INVISIBLE (drawn by a block entity renderer that shows the owner name).
- **Particles:** 4 ASH particles per tick at the top.
- **No edit actions** (shift does nothing). No menu on placement.
- **Waystone record:** type `fleeting_memorial`, named "Fleeting Memorial", owner = the player who died.
- **Spawning:** when a player dies with an `epitaph` anywhere in their inventory:
  - The memorial is placed at death position +1. If that is not free, it searches radius 1–2 and dy 0–3 for a replaceable block.
  - One epitaph is consumed.
  - The client shows the totem animation with the epitaph item and plays the TOTEM_USE sound.
- **Use:** listed as a target only for its owner. After a **successful** teleport to it, it is destroyed and the listing removed.

---

## 2. Items

| Id | Name | Stack | Recipe | Behaviour |
|---|---|---|---|---|
| `warp_stone` | Warp Stone | 1, durability 128 (Unbreaking/Mending via `enchantable/durability`) | Shaped `DED / EGE / DED`: D = amethyst_shard, E = ender_pearl, G = `c:gems/emerald` | Glint. Hold right-click for `warpStoneUseTime` (32 t), BOW animation. Opens selection list (targets for an item). Loses 1 durability after a successful warp if `enableDurability`. Hand-anchored particle effects; taking damage cancels the charge. |
| `warp_scroll` | Warp Scroll | 64 | Yields 3. Shaped `GDG / GEG / PPP`: G = `c:nuggets/gold`, D = ink_sac, E = ender_pearl, P = paper | Charge for `scrollUseTime` (32 t), BOW animation, PORTAL_TRIGGER sound (0.1 vol, pitch 2). Opens selection list. Consumed after a successful teleport. |
| `return_scroll` | Return Scroll | 64 | Yields 3. Shaped `GEG / PPP`: G = gold nugget, E = ink_sac, P = paper | Teleports to the return portal if one exists, otherwise to the **nearest activated waystone in the same dimension**. Consumed on success. Glint. FOV zoom while charging. |
| `blank_scroll` | Blank Scroll | 64 | Yields 3. Shaped `GFG / PPP`: G = gold nugget, F = feather, P = paper | Use on any waystone-type block entity (waystone, sharestone, warp plate) to consume 1 and get a `bound_scroll` bound to that block. Tooltip: "Use this scroll on a waystone to bind it." |
| `bound_scroll` | Bound Scroll | 64 | — | Glint. Tooltip: "Bound to: <name>" (name in aqua). Charge time 32 t, then teleports to its bound waystone. Consumed on success. Errors are shown dark red in the action bar. |
| `portal_scroll` | Portal Scroll | 64 | Yields 3. Shaped `SIS / SES / PPP`: S = amethyst_shard, I = ink_sac, E = ender_pearl, P = paper | Glint. Charge; fails with "There is no space to open a Warp Portal here." if there is no room. Otherwise opens the selection list; picking a target spawns a Warp Portal and consumes the scroll. |
| `warp_dust` | Warp Dust | 64 | Shapeless: ender_pearl + amethyst_shard gives 4 | Crafting ingredient. |
| `dormant_shard` | Dormant Shard | 64 (1 in a plate slot) | Shapeless: 2 warp_dust + flint | Put into a warp plate to attune it. |
| `attuned_shard` | Attuned Shard | 1 | (made by a plate) | Glint if its target is valid. Tooltip: target name (galactic name for plates, light purple for others), then "Insert this shard into a warp plate.", or "This shard has lost its attunement." |
| `crumbling_attuned_shard` | Crumbling Attuned Shard | 4 | **No recipe in 21.1.46** (creative/API only) | Single-use. Tooltip adds "It will be consumed on next teleport." (white italic). |
| `deepslate_shard` | Deepslate Shard | 64 | Shapeless: deepslate + flint | Locks a warp plate. |
| `twinbound_feather` | Twinbound Feather | 1 | Shapeless: feather + amethyst_shard + gold_nugget + ink_sac | See §3.12. |
| `epitaph` | Epitaph | 1 | Shaped `GGG / ADA / GGG`: G = gold nugget, A = amethyst_shard, D = deepslate | Spawns a Fleeting Memorial on death. |

- **Data components:**
  - `waystone` (UUID, on block items)
  - `attunement` (UUID)
  - `boundScroll` {waystoneId, waystoneName}
  - `twinboundFeather` (UUID)
  - `twinboundFeatherLinks` (set of UUIDs)
- **Creative tab "Waystones"**, icon = waystone. Order: waystone, white_portstone, red_sharestone, warp_plate, blank_scroll, return_scroll, warp_scroll, portal_scroll, warp_stone, warp_dust, dormant_shard, twinbound_feather, epitaph, deepslate_shard, then the other `*_waystone`, `*_sharestone` and `*_portstone` blocks, attuned_shard, crumbling_attuned_shard.
- **Item tags:**
  - `scrolls`, `bound_scrolls`, `return_scrolls`, `warp_scrolls`, `portal_scrolls`, `warp_stones`, `waystones`, `sharestones`
  - `warp_shards`: attuned, crumbling, dormant, deepslate
  - `single_use_warp_shards`: crumbling
  - `warp_modifiers`: blaze_powder, poisonous_potato, ink_sac, milk_bucket, honey_block, diamond, feather, magma_cream, wither_rose, quartz, spider_eye, redstone_torch
  - `warp_modifiers/redstone_sensitive`: redstone_torch
- **Block tags:**
  - `waystones` (10 waystone variants), `sharestones`, `portstones`
  - `is_teleport_target`: all waystones, plates, portstones, sharestones, portal, memorial. Used to check that a target still exists.
  - `mineable/pickaxe`
  - `c:relocation_not_supported`

---

## 3. Mechanics

### 3.1 Waystone data model
- **Record** (stored in SavedData `Waystones`), NBT fields: `WaystoneUid`, `Type`, `NameV2` (component JSON), `World`, `BlockPos`, `Transient`, `Origin`, `OwnerUid`, `OwnerUsername`, `Visibility`.
- **Per-player data** in the player's persistent NBT `WaystonesData`:
  - `Waystones` (activated list)
  - `SortingIndex`
  - `WaystoneSortMode`
  - `Cooldowns` (key → epoch ms)
  - `Aliases`
  - `Groups` (per-waystone group ids)
  - `HiddenWaystones`
  - `GroupRegistry` (Id, Name, Icon, Color, Inbuilt, Hidden, SortIndex)
- **Name generator** SavedData `waystones_NameGenerator`: a `UsedNames` list. Duplicates get a Roman numeral suffix (" II", " III"...).
- **"Personalized" view:** effective name = alias if set, otherwise the name.

### 3.2 Activation and discovery
- **Right-clicking an unactivated waystone:**
  - Activates it for that player.
  - Chat message: "Activated Waystone: <name>". The message is yellow; the name is white.
  - Sound: PLAYER_LEVELUP at volume 0.2.
  - Particles: 2×32 ENCHANT particles that fall from y+3 and y+4.
  - Stat `waystones:waystone_activated` is incremented.
  - Observers are pulsed.
- **Right-clicking an activated waystone:** opens the selection menu.
- **Only the `waystone` type is stored as "activated".** Sharestones and plates do not need activation.
- **First activation of a generated waystone** (wild/village, no name):
  - It gets a generated name. Default template `{MrPork}`: syllable tables 1+2+3, e.g. "Krmiark".
  - If it has no owner, **the activator becomes the owner** and its visibility is set to the default.
- **Player-placed waystones are not auto-named.** The name stays empty until edited. A save with an empty name becomes "Untitled Waystone".

### 3.3 Visibility (`WaystoneVisibility`)
- **Values:**
  - `ACTIVATION`: visible to players who activated it.
  - `GLOBAL`: visible to everyone; listed for all players without activation.
  - `TEAM`: visible to members of the owner's scoreboard team.
  - `SHARD_ONLY`: warp plates.
  - 15 × `<COLOR>_SHARESTONE`.
- **Options offered in the Edit screen** (`WaystoneVisibilities`):
  - Waystones: [configured default, ACTIVATION, TEAM, GLOBAL (only if the player may manage global waystones)].
  - Sharestones and plates: only their fixed visibility, so the button is inactive.
- **Global and team indexes** are kept in memory and rebuilt on server start. Waystones that were generated but never named are excluded.

### 3.4 Ownership and permissions
- **Permission `waystones:edit_all`:** granted by default to players in **creative** (`instabuild`). It bypasses all edit checks.
- **Edit is denied when:**
  - The waystone has an owner, `restrictedWaystones` contains PLAYER (default), and the player is not the owner. Message: "Only the owner may edit this waystone."
  - Its origin is non-PLAYER and that origin is in `restrictedWaystones`.
  - It is GLOBAL and the player can't manage global waystones. Message: "Only creative users may edit this waystone."
- **Who may manage global waystones:** creative players, or everyone if `allowEveryoneToManageGlobalWaystones` is set, `defaultVisibility` is GLOBAL, or the deprecated `allowedVisibilities` contains GLOBAL.
- **Server checks on save:**
  - Re-checks the edit permission.
  - Requires the player to be within 4 blocks of the block.
  - An invalid visibility falls back to the first option.
  - Empty name becomes "Untitled Waystone".
  - If the name equals the configured inventory-button target name and a waystone with that name already exists, `*` is appended.
- **When editing is denied**, the Edit screen still opens read-only and shows the error line.

### 3.5 Teleport pipeline
1. **Build the context:** entity, target, from-waystone, warp item and hand, flags. Flags are `INVENTORY_BUTTON`, `PORTSTONE` and `ADMIN`.
   - Pets are added if `transportPets` allows it: tamed, owned by the player, within 10 blocks, not sitting, not leashed, not on the deny-list.
   - Leashed mobs within 10 blocks are added.
   - Requirements are resolved from `warpRequirements`.
2. **Validate at start:**
   - Pre-event not cancelled.
   - Entity is alive and connected.
   - From-waystone is in the same dimension and within interaction range (player `canInteractWithBlock(pos, 4)`; non-players within 8 blocks).
   - The warp item is still held.
   - Leash rules: DISABLED gives "You cannot take leashed mobs with you"; a denied entity gives "You cannot take this mob with you"; SAME_DIMENSION across dimensions gives "...between worlds".
   - Requirements are affordable, or the player is in creative. Otherwise "You do not have enough experience to teleport."
3. **Asynchronously load the destination chunks:** the target's chunk plus the 4 neighbouring chunks.
4. **Validate again:** entities, source range, item, dimension, target still valid, and the block at the position is in `is_teleport_target`. If the block is missing: "The waystone you tried to teleport to is currently being moved or has gone missing."
5. **Consume requirements, then teleport:**
   - **Destination:** the block next to the waystone in its `facing` direction, if that block and the one above are not suffocating. Fallback order: E, W, S, N, then the waystone position itself. Centred at +0.5 on each axis. Yaw = that direction.
   - Mounts and passengers are teleported together and re-mounted. Leashes are re-attached.
   - Vertical velocity is zeroed and `onGround` set, unless the entity is elytra-flying.
4. **Effects:**
   - PORTAL_TRAVEL sound at the source (volume 0.01) and at the target (volume 0.05).
   - 128 PORTAL particles within ±1.5 blocks at the source and the target.
   - Modifier effects from the **target's** container are applied (§3.10).
5. **Error messages** are shown dark red in the action bar.
- **Menus close** when the player moves more than 4 blocks from the source waystone (`stillValid`).

### 3.6 Warp requirements (costs and cooldowns) **[21.1.46]**
**Default `warpRequirements`, applied in order:**
```
[is_not_interdimensional] scaled_add_xp_cost(distance, 0.01)
[is_interdimensional] add_xp_cost(27)
[source_is_warp_plate] multiply_xp_cost(0)
[source_is_scroll] multiply_xp_cost(0)
[target_is_global] multiply_xp_cost(0)
[target_is_fleeting_memorial] multiply_xp_cost(0)
min_xp_cost(0)
max_xp_cost(27)
[source_is_inventory_button] add_cooldown(inventory_button, 300)
```
- **Units:** costs are in **XP points**, not levels. 1000 blocks = 10 points. Changing dimension = 27 points, which is also the cap. Scrolls, plates, global targets and memorials are free. The inventory button has a 300-second cooldown.

**Syntax:** `[cond1, cond2(arg)] function(args)`.
- Ids default to the `waystones:` namespace.
- Every condition automatically gets a negated `*_not_*` form: the word `not_` is inserted after the first `is_`, or prefixed if the name has no `is_`.

**Functions:**
- **Level cost:** `add_level_cost(f)`, `multiply_level_cost(f)`, `scaled_add_level_cost(var, f)`, `scaled_multiply_level_cost(var, f)`, `min_level_cost(i)`, `max_level_cost(i)`.
- **XP-point cost:** `add_xp_cost(i)`, `multiply_xp_cost(f)`, `scaled_add_xp_cost(var, f)`, `min_xp_cost(i)`, `max_xp_cost(i)`.
- **Cooldown:** `add_cooldown(id, s)`, `multiply_cooldown(id, s)`, `scaled_add_cooldown(var, id, s)`, `scaled_multiply_cooldown(var, id, s)`, `min_cooldown(id, s)`, `max_cooldown(id, s)`.
- **Soft cooldown** (always affordable, only sets the timer): the same six functions with `soft_cooldown`.
- **Item cost:** `add_item_cost(item, n)`, `multiply_…`, `scaled_add_…(var, item, n)`, `scaled_multiply_…`, `min_…`, `max_…`.
- **Other:** `dismount()`, and `refuse(msg)`. In `refuse`, a message starting with `$` is a translation key; it is always unaffordable and shows a cancel icon.
- **Kill switches:** cost functions are disabled by `enableCosts=false`, cooldown functions by `enableCooldowns=false`.

**Conditions:**
- **Source:** `source_name_equals(s)`, `source_name_contains(s)`, `source_is_warp_plate`, `source_is_portstone`, `source_is_waystone`, `source_is_sharestone`, `source_is_<color>_sharestone`, `source_is_inventory_button`, `source_is_scroll`, `source_is_bound_scroll`, `source_is_return_scroll`, `source_is_warp_scroll`, `source_is_portal_scroll`, `source_is_warp_stone`, `source_is_dimension(id)`.
- **Target:** `target_name_equals`, `target_name_contains`, `target_is_warp_plate`, `target_is_warp_portal`, `target_is_fleeting_memorial`, `target_is_player`, `target_is_global`, `target_is_sharestone`, `target_is_<color>_sharestone`, `target_is_waystone`, `target_is_dimension(id)`.
- **Context:** `is_interdimensional`, `involves_dimension(id)`, `is_within_distance(f)`.
- **Entity and riding:** `is_on_any_vehicle`, `is_on_vehicle(id or #tag)`, `is_with_passengers`, `is_with_pets`, `is_with_leashed`.
- **Player state:** `has_cooldown(id)`, `has_cooldown_above(id, s)`, `has_item(item, n)`, `has_empty_inventory`, `is_wearing_any_armor`, `has_tag(s)`.

**Variables:** `distance`, `leashed`, `pets`, `passengers`.

**Cooldowns:**
- Stored per player as wall-clock ms, synced to the client.
- In creative, using the inventory button resets them.
- Default cooldown id: `waystones:inventory_button`.

**Interdimensional travel:** always allowed in 21.1.46. The `DimensionalWarp` enum (ALLOW/GLOBAL_ONLY/DENY) still exists in code but is unused. To deny dimension changes, use e.g. `[is_interdimensional] refuse($chat.waystones.cannot_dimension_warp)`.

**Legacy defaults:**
- **[21.11]:** the requirement list includes `[source_is_warp_stone] add_durability_cost(80)` and has no scroll or memorial lines.
- **[26.x]:** split into `warpSettings` and `warpRequirements` using Shogi rule syntax:
  - `warpSettings`: `$xp_per_block = 0.01`, `$min_xp_cost = 0`, `$max_xp_cost = 27`, `$interdimensional_xp_cost = 27`, `$inventory_button_cooldown = '300s'`.
  - `warpRequirements`:
    - `$uses_xp` applies only when the source is a waystone or warp stone and the target is not a memorial.
    - `is_warp_stone -> damage_item(80)`.

### 3.7 Selection target sets
| Source | Targets listed |
|---|---|
| Waystone | Activated waystones + global/team waystones + bound scrolls/shards in the source's container + the player's memorials + Twinbound Feather players + return portal |
| Sharestone | All sharestones of that colour (+ container items, memorials, return portal) |
| Portstone (white) | Activated waystones + global/team |
| Portstone (colour) | That colour's sharestones |
| Warp Scroll / Warp Stone | Activated + global/team + memorials + Twinbound players + return portal |
| Portal Scroll | Activated + global/team + memorials |
| Inventory button (ANY) | Activated + global/team + memorials + Twinbound players |

The current (source) waystone is shown but disabled.

### 3.8 Inventory button
- **Config `inventoryButton`:**
  - `""` or `NONE`: disabled. **This is the default.**
  - `NEAREST`: the nearest activated waystone in the same dimension, preferring a return portal.
  - `ANY`: opens the selection menu.
  - `<waystone name>`: a specific waystone, matched by name among activated ones.
- **Position:**
  - Survival inventory: `inventoryButtonX = 58`, `inventoryButtonY = 60`, relative to the inventory's top-left.
  - Creative: `creativeInventoryButtonX = 88`, `creativeInventoryButtonY = 33`, shown only on the survival-inventory tab.
- **Appearance:** 16×16.
  - Normally shows the bound-scroll item icon; on hover, the warp-scroll icon.
  - If unaffordable (e.g. on cooldown), shows the `inventory/inventory_button` sprite at 50% alpha. Clicking then just plays the UI click sound.
- **Tooltip:**
  - First line: "Return to Waystone ..." for named/ANY modes, or "Return to Nearest Waystone" (yellow).
  - Second line: "Bound to: <name>" (gray, name dark aqua), or "You have not activated any waystones yet." (red).
  - If unaffordable, the requirement tooltip is added: "Cooldown: N seconds" (gold).
- **Click:**
  - NEAREST or named: opens a **ConfirmScreen**. Title "Do you really want to warp back to this waystone?", vanilla Yes/No buttons, and the line "Bound to: <name>" centred at y=100. Yes sends the request to the server.
  - ANY: opens the selection screen directly.

### 3.9 Sharestone groups
Every sharestone of the same colour can reach every other one, server-wide. No activation is needed. Only creative players can delete them from the list.

### 3.10 Modifiers (5-slot container on waystones and warp plates)
- Slot 0 accepts a dormant shard (stack size 1). Slots 1–4 accept any item.
- Effects are applied to entities **arriving** at that block.
- `potency` starts at 1. Each diamond adds 1, up to 4. Potency is the effect amplifier.

| Modifier | Effect on arriving entities |
|---|---|
| Blaze powder | Set on fire, 1 s per item |
| Poisonous potato | Poison, 1 s per item |
| Ink sac | Blindness, 1 s per item |
| Feather | Slow Falling, 1 s per item, max 8 s |
| Magma cream | Fire Resistance, 1 s per item, max 8 s |
| Wither rose | Wither, 1 s per item |
| Milk bucket / honey block | Cure effects |
| Quartz | Round-robin target selection (plates) |
| Spider eye | Prefer single-use shards (plates) |
| Redstone torch | Plate is disabled while powered |
| Amethyst shard / slime ball | Faster / slower plate use time |

- **Kill switch:** `enableModifiers`.
- **"N modifiers active" count** includes every item in the table **except amethyst shards and slime balls**, which only change plate timing.
- **Access:** the modifier menu opens from the Edit screen. The server only checks distance (≤ 8 blocks), not permission. The client disables the button when editing is not allowed.

### 3.11 World generation
- **Wild waystones** are a feature per style, placed by the custom `waystone_placement` placement modifier.
  - One per grid cell of `chunksBetweenWildWaystones` (default 25) chunks, with a seeded random offset of up to `ceil(n/2)`.
  - Placed on the OCEAN_FLOOR_WG heightmap. In dimensions with a ceiling it scans down for the first air-above-solid position.
  - Default dimension allow-list: overworld, the_nether, the_end.
  - Random facing. Origin WILDERNESS.
- **Style by biome** (`wildWaystoneStyle = BIOME`), via biome tags `has_structure/<style>`:
  - desert → sandy
  - swamp, mangrove swamp, mushroom fields, jungles → mossy
  - nether biomes → blackstone
  - end biomes → end stone
  - deep_dark → deepslate
  - most other overworld biomes and oceans → default
  - The purpur, prismarine, nether-brick and mud-brick tags are empty.
- **Villages:**
  - The waystone structure is added to `village/{plains,snowy,savanna,taiga}/houses` with weight 1 (desert uses its own `village/desert/waystone`).
  - There is also a waterlogged structure variant.
  - `FREQUENT` forces one waystone piece per village via a mixin.
  - Data-driven integrations exist for Repurposed Structures and Lithostitched.
  - Origin VILLAGE.
- **Naming modes:** `PRESET_FIRST`, `RANDOM_ONLY`, `PRESET_ONLY`, `MIXED`. Template placeholders: `{MrPork}` and `{Biome}`.

### 3.12 Twinbound Feather **[21.1.46+]**
- **Linking:**
  - Two players each hold use (BRUSH animation) while looking at each other within 2 blocks, both using feathers.
  - After 20 ticks: HEART particles at the midpoint, then the ENCHANTMENT_TABLE_USE sound.
  - Each feather gets the other's id.
- **Use:** while both carry their linked feathers (inventory, offhand or trinket slot) and are online, each appears as a target ("<player>", group Players) in waystone menus, scroll/stone menus and the inventory button. Teleporting goes to the other player's current position. The requirement tooltip shows the feather name in light purple.

### 3.13 Misc
- **Sync on login:** sort mode, sorting index, groups, activated waystones, all warp plates, all sharestones, cooldowns.
- **Taking damage** while charging a scroll or warp stone cancels the charge.
- **Map-mod integrations:** JourneyMap, Dynmap, BlueMap, Jade/WTHIT. Transient (silk-touched) waystones are hidden from maps.

---

## 4. UI screens (modern UI) **[21.1.46]**

All screens are container screens with **no background texture**: widgets draw over the dimmed world. The only exception is the Warp Modifiers screen.

### 4.1 Waystone selection screen
- **Used by:** waystone, warp scroll, warp stone, portal scroll, portstone, inventory menus. The sharestone and admin menus use the same screen with small variations.
- **Size:** width 270. Height = `max(200, needed)`, capped at `screenHeight − 40`.
- **Header (64 px):**
  - Title "Select your destination", centred. It sits at y=0 if there is a source waystone, otherwise y=20.
  - "Current Location: <name>" at y=20. The prefix is yellow, the name white. On hover the name is underlined and a 16×16 edit icon appears; clicking opens the Edit screen for the source.
  - Search box at y=40, width 220 (or 198 when the sort button is present), height 20. It filters by effective name, case-insensitive.
  - Sort button 20×20 to the right of the search box. It cycles **Manual → By Name → By Distance**, with icons `sort_manual`/`sort_name`/`sort_distance` and tooltip "Sort: X". The choice is persisted on the server.
    - Name sort is case-insensitive.
    - Distance sort puts the same dimension first, then nearest.
    - Manual uses the per-player sorting index.
    - Return portals and memorials are always pinned first.
- **Left column at `leftPos − 8`:**
  - "Manage Waystones" icon button 20×20. The admin menu hides it.
  - Below it (+5 px), **group filter buttons**, 20×20 each with a 2 px gap. Only non-hidden groups that contain at least one shown waystone get a button. Up to 5, plus one per extra 22 px row of height.
  - Each shows the group icon with the group name as tooltip. Clicking toggles the filter; the active one gets a white outline.
- **List:** scrollable. Row height 22, row width 220, button 216×20.
  - **Label:** effective name, coloured with the first matching group's colour. "<unnamed>" if empty. Warp-portal entries are light purple.
  - **Left side:** requirement widgets.
    - XP: the enchanting-table `level_1/2/3` sprite for the level cost (computed from points), a disabled variant if unaffordable, and "+" if more than 3 levels.
    - Cooldown: "mm:ss" in light red.
    - Item cost: item icon with count, half-transparent if missing.
    - Refuse: cancel sprite.
    - Hovering this area shows the requirement tooltip: "Experience Cost: N" (green), "Level Cost: N", "Cooldown: N seconds" (gold), "Item Cost: %d %s", refuse message (red).
  - **Right side:** distance "123m", or "1.2km" when ≥10,000 m or when the name is long. Only shown for the same dimension. A 20 px dimension overlay sprite marks Nether and End entries.
  - **Disabled rows:** the source waystone, and anything the player can't afford (unless creative).
  - **Empty list:** "You have not activated any waystones yet." or "There are no matching Sharestones to teleport to." in red.
- **Click:** sends the selection. The server validates that the waystone is in the menu list, closes the menu and teleports.

### 4.2 Manage Waystones screen
- Same frame. Title "Manage Waystones". No sort button.
- **Left column:** Back button (icon, tooltip "Back") at y=40, and "Manage Groups" button at y=64.
- **Row layout:**
  - Drag handle, 16 px. Tooltip: "Drag to reorder. Double-click to move to top. Shift-double-click to move to bottom."
  - Disabled name button.
  - Edit (alias) icon, 13×13, tooltip "Personal Settings".
  - Then either the delete X (13×13) or an eye toggle ("Hide Waystone"/"Show Waystone"). The eye toggle appears for global, team or sharestone entries that this player cannot delete.
- **Delete X:** only active while **Shift** is held. Tooltips: "Hold shift and click to delete" / "Click to delete". For global waystones and sharestones a red line is added: "This will remove the waystone for everyone."
- **Who can delete:**
  - Waystone-type entries.
  - Not global, unless creative or global management is enabled.
  - Team waystones only if activated.
  - Sharestones only in creative.
- **What delete does:**
  - Survival: forget (deactivate) for that player.
  - Creative + sharestone: purge from the database.
  - Creative + global: demote to ACTIVATION, and purge if the block no longer exists.
- **Dragging:** auto-scrolls within 12 px of the list edge. Each step sends a swap packet.
- Return portals and memorials are excluded from this list.

### 4.3 Personal Settings screen
- Width 176. Title = waystone name at y=44.
- **Widgets:**
  - Alias field: 150×19 (176 wide if editing is unavailable). Max 128 characters. Placeholder "No alias". Right-click clears it.
  - "Edit Waystone" pencil button 21×21 at x+155. Only shown within 4 blocks, and not for feather targets.
  - **Group cycle button**, width 150. Left-click goes to the next group, right-click to the previous. Shows the icon and the coloured name, or "No Group" / "No groups defined". Only user (non-inbuilt) groups are offered.
  - "Manage Groups" button 20×20.
  - "Favorite" checkbox.
  - "Save" button 100×20.
- **Closing:** Enter or Esc saves and returns.

### 4.4 Manage Groups screen and Edit Group screen
- **Manage Groups:** search box plus a "+" Create Group button, and a Back button.
  - Row layout: drag handle | icon + coloured name | edit icon | delete X (user groups, Shift-gated) or hide/unhide toggle (inbuilt groups: "Hide Inbuilt Group"/"Unhide Inbuilt Group").
  - Empty list: "There are no groups available."
- **Inbuilt groups:**
  - Favorites: gold, sort index −30.
  - Players: green, −20.
  - Global Waystones: yellow, −10.
  - Teams: aqua, −5.
  - Dynamic dimension groups: Overworld, The Nether, The End, shown in white.
- **Edit Group** (176×120, title "Edit Group"):
  - Name field, text in the group colour, placeholder "Unnamed Group".
  - Icon button 21×21 cycling the preset icons: community_hubs, dimension, dungeons, favorites, global, player_homes, players, resource_sites, teams, villages, plus the dimension icons. Right-click goes backwards. Tooltip "Icon: X".
  - Colour button 21×21 cycling 15 colours (white, gold, light_purple, aqua, yellow, green, red, gray, dark_gray, dark_aqua, dark_purple, blue, dark_blue, dark_green, dark_red). Tooltip "Label Color: X".
  - Save button.
  - New group ids are `waystones:user/<uuid>`.

### 4.5 Edit Waystone screen (settings)
- **Opened by:** shift + right-click, placement, or clicking the location header.
- **Title:** "Edit <Waystone|Sharestone|Warp Plate>", centred at y=44.
- **Widgets, top to bottom:**
  1. Optional red error line at y=56.
  2. Name field 150×19, max 128 characters, placeholder "Untitled Waystone". Read-only if editing is denied; right-click clears it.
  3. Personal Settings button 21×21 at x+155.
  4. Visibility icon button 20×20 (sprites for activation, global, team, shard_only, sharestone) with a label to its right ("Visible upon Activation", "Visible to Everyone", "Visible to Team", "Teleports via Attuned Shard", "Visible to <Color> Sharestones"). Clicking cycles through the allowed options. If editing is denied, the tooltip adds "This waystone cannot be edited." in red.
  5. Manage Modifiers button 20×20 with label "N modifiers active" (green) or "No modifiers active" (gray).
  6. "Save" button, or "Close" if editing is denied. 100×20, centred.
- **Closing:** Esc, Enter or the button sends name and visibility to the server.

### 4.6 Warp Modifiers screen
- Texture `textures/gui/menu/waystone_modifiers.png`, 176×196. Title "Warp Modifiers". Galactic name at the top right.
- **Slots in a cross:**
  - Centre (80,45): shard slot 0.
  - Up (80,17), right (108,45), down (80,73), left (52,45).
- Player inventory at y=104; hotbar at y=162; inventory label at y=93.
- **Shift-click:** puts a single item into slot 0 if it is empty, otherwise into slots 1–4.

### 4.7 Classic UI **[21.11 and earlier 21.x]**
- No groups, aliases, sort modes or Manage screen.
- **Paged list:**
  - Rows 22 px apart, buttons 200×20 at x = centre − 100.
  - Global waystones are labelled in yellow.
  - Up/down arrow buttons (vanilla `server_list/move_up`/`move_down` sprites, 11×7) at centre + 108. Shift-click moves to the top or bottom.
  - Delete X at centre + 122 (Shift-gated).
  - Previous/Next buttons, 95×20. Shift jumps to the first or last page.
  - Search box 198 wide.
  - Hovering the location header shows a writable-book icon.
- **Edit screen:** a full 176-wide name field, without the personal-settings button.

---

## 5. Configuration **[21.1.46]** (file `waystones`, "Synced" means sent to clients)

| Section | Key | Default |
|---|---|---|
| general | restrictedWaystones (set of origin) | [PLAYER] |
| general | defaultVisibility (ACTIVATION/GLOBAL/TEAM) | ACTIVATION |
| general | allowEveryoneToManageGlobalWaystones | false |
| general | allowedVisibilities (deprecated) | [] |
| general | warpStoneUseTime (ticks) | 32 |
| general | warpPlateUseTime | 15 |
| general | warpPlateCooldownTime | 20 |
| general | scrollUseTime | 32 |
| teleports | enableCosts | true |
| teleports | enableCooldowns | true |
| teleports | enableDurability | true |
| teleports | warpRequirements | list in §3.6 |
| teleports | transportPets (ENABLED/SAME_DIMENSION/DISABLED) | DISABLED |
| teleports | transportLeashed | ENABLED |
| teleports | entityDenyList | [minecraft:wither] |
| teleports | enableModifiers | true |
| inventoryButton | inventoryButton | "" (off) |
| inventoryButton | inventoryButtonX / Y | 58 / 60 |
| inventoryButton | creativeInventoryButtonX / Y | 88 / 33 |
| worldGen | wildWaystoneStyle (DEFAULT, MOSSY, SANDY, BLACKSTONE, RED_NETHER_BRICKS, DEEPSLATE, END_STONE, PURPUR, PRISMARINE, MUD_BRICKS, BIOME) | BIOME |
| worldGen | chunksBetweenWildWaystones (0 disables) | 25 |
| worldGen | wildWaystonesDimensionAllowList | overworld, the_nether, the_end |
| worldGen | wildWaystonesDimensionDenyList | [] |
| worldGen | nameGenerationMode | PRESET_FIRST |
| worldGen | nameGenerationTemplate | `{MrPork}` |
| worldGen | nameGenerationPresets | [] |
| worldGen | spawnInVillages (DISABLED/REGULAR/FREQUENT) | REGULAR |
| client | disableTextGlow | false |
| compatibility | journeyMap / preferJourneyMapIntegrationMod / dynmap / fixVanillaTeleportBug | true / true / true / false |
| blueMap | enabled / includeWaystones / includeSharestones / includeUndiscoveredWaystones | true / true / true / false |

- **Version notes:**
  - **[21.11]:** enum values are lowercase. `defaultVisibility` is the full `WaystoneVisibility` type, and `allowedVisibilities` is still the active option (it is not deprecated there).
  - **[26.x]:** settings are grouped as `rules`, `journeyMap`, `blueMap`, `dynmap`. `enableCosts` is renamed `enableXpCosts`, and a `warpSettings` list is added.
- **Very old versions (1.16–1.20, pre-requirement system):** level-based costs (`blocksPerXPLevel`, `maximumBaseXpCost`, `dimensionalWarpXpCost`, global multiplier) and a `dimensionalWarp` option. These are not in 21.x.

---

## 6. Commands
- **Root:** `/waystones`. Every subcommand needs permission level 2, with permission nodes `waystones:command.waystones.<sub>`.
- **Subcommands:**
  - `activate <targets> <pos>`: activates the waystone at `pos` for the targets. Output: "Activated waystone %s for %s" or "... for %d players".
  - `forget <targets> <pos>` and `forget <targets> all`: "Deactivated waystone %s for %s", "Deactivated all waystones for %s" (plus "multiple" variants).
  - `place <pos> <style> <name…>`: style suggestions are `waystone`, `mossy_waystone`, etc. Unknown style: "Unknown waystone style: x".
  - `count <player>`: "%s has %d waystones activated and owns %d of those".
  - `list <player> [all]`:
    - Header "Waystones (x, y, z) by %s (owned only):" or "Waystones (x, y, z) by %s:".
    - Entry "-     owned %s (%s): %s" or "- activated %s (%s): %s". The name is green; the coordinates are yellow and clickable, suggesting `/execute in <dim> run teleport x y z`. The location shows "in <dim>" or "at N blocks away".
    - Footer "%d waystones owned" or "%d waystones total, %d owned".
  - `gui <player>`: opens the **Admin selection screen**. Title "Select which waystone of <%s> you would like to visit:". No manage, sort or delete controls.
  - `twinbound <first> <second>`: links (or gives) feathers. Messages: success, already_linked, same_player, no_space.
  - `cooldown <targets> reset all | reset <id> | add <id> <seconds> | set <id> <seconds≥0>`.
- **Error:** "Waystone not found at this position".

## 7. Translation strings (en_us)
The key families are:
- `block.*`, `item.*`
- `container.waystones.*`
- `gui.waystones.waystone_selection.*`, `gui.waystones.manage_waystones.*`, `gui.waystones.manage_groups.*`, `gui.waystones.waystone_settings.*` (including `visibility.*`), `gui.waystones.personal_waystone_settings.*`, `gui.waystones.group_settings.*`, `gui.waystones.inventory.*`
- `chat.waystones.*`
- `tooltip.waystones.*`
- `commands.waystones.*`
- `color.waystones.group.*`, `waystones.groups.*`
- `waystones.configuration.*`
- `stat.waystones.waystone_activated` = "Waystones Activated"

The full upstream file (about 360 keys) is `common/src/main/resources/assets/waystones/lang/en_us.json` on the `1.21.1` branch of TwelveIterationMods/Waystones; check exact English wording there. The most important values are quoted inline in the sections above.

## 8. What changes in 26.x
- **Materials replace dye colours** for sharestones, portstones and warp stones: copper, prismarine, gold, diamond, amethyst, lapis, emerald, redstone, plus a "ruined" sharestone. Warp stones become typed per kind.
- **Renamed waystone ids:** `andesite_waystone` (was `waystone`), `mossy_andesite_waystone`, `sandstone_waystone`.
- **Rules** move to the Shogi library.
- **New:** a config screen with a rule editor, a "Recipe input" for waystones, and a `hidden-waystones`/`aliases` feature set.

For a port that targets 1.21.x behaviour, follow 21.1.46.

**Suggested implementation units:**
1. Data store and sync
2. Blocks (waystone, sharestone, portstone, plate, portal, memorial)
3. Items
4. Teleport pipeline
5. Requirement DSL and cooldowns
6. Permissions and visibility
7. Selection UI
8. Manage, personal, groups UIs
9. Edit and modifier UIs
10. Inventory button
11. World generation and naming
12. Commands
13. Feather and epitaph
14. Configuration
15. Localisation
