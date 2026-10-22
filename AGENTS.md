# AGENTS.md

Bedstones is a Bedrock port of the *behaviour* of the Java mod **Waystones 21.1.46** (BlayTheNinth),
written as a LeviLamina native mod. It targets MC **1.26.51** / LeviLamina **26.51.7**, builds with
xmake + clang-cl on Windows x64, and runs on **both** the dedicated server (BDS) and the client
(local worlds through LeviLauncher). One code base, two builds (`--target_type=server|client`).

The behaviour reference is `docs/SPEC.md` (blocks, items, mechanics, every screen, config,
commands, strings). Read it before working on a unit.

Policy: never copy code or assets from the Java Waystones repository (behaviour reference only).
Textures are original pixel art drawn by `tools/textures/*.ps1`. English UI strings should match
Waystones' wording (they are behaviour), written into our own lang files.

## Layout

| Path | What it is |
| ---- | ---------- |
| `src/core/` | Pure C++ (no `mc/`/`ll/` includes): `Waystone.h` (WaystoneRecord, Visibility, Origin, types, PlayerWaystoneData, groups, dye colors, uuid, glob), `Warp.h` (WarpSource, WarpTarget, Requirement, XP maths), `Format.h`. Static lib `bs-core`, unit-tested in `tests/`. |
| `src/game/` | Game adapters: `Ids.h` (every block / item id, block state names, item NBT keys), `Waystones` (the world's waystone registry + player data), `WorldDb` (LevelDB JSON + world lifecycle), `Blocks` (states, two-tall placement, facing), `Items` (stacks, user NBT, inventory), `Effects` (action bar, sounds, particles). |
| `src/service/` | **Service seams** (`Services.h`) with defaults (`Defaults.cpp`) and `TargetProviders.h`. |
| `src/net/` | Companion protocol (both builds): `Packets.h`, `Companion` (handshake), `Screens` (server-authoritative screens + form fallbacks), `Messages.h` (named JSON channels). |
| `src/client/` | Client mod (client build only, files wrapped in `#ifdef BS_CLIENT`): `ui/gfx/Painter`, `ui/UiRoot` + `UiLayer` (layers drawn over any screen), `ui/screen/Screens.h` (native screens + host), `ui/widgets/` (`Style` = vanilla look, `Widgets` = Button / Checkbox / ScrollList / HoverTooltip, `TextBox`, `Tooltip`), `input/InputRouter`, `net/ClientNet`. |
| `src/mod/` | Entry (`Main.cpp`), `Log.h`, `Module.h`, `Config.h`, `ModConfig.h`, `I18n.h`, `Commands.h`, `Registry.h`. |
| `src/selftest/` | In-game test framework (`SelfTest.h`), foundation tests, `/waystones debug screen`. |
| `src/pack/` | `PackDeployer` (BDS: puts the resource pack on the world). |
| `src/feature/<unit>/` | Feature units (gameplay, server side; may contain `#ifdef BS_CLIENT` parts). |
| `src/client/screens/<unit>/` | Native screens of a unit (client build). |
| `assets/lang/<locale>/<unit>.json` | Mod translations (server + client mod text), merged into `lang/<locale>.json`. |
| `assets/pack/bp/` | Behavior pack: `blocks/`, `items/`, `recipes/`, `loot_tables/`, `manifest.json`. |
| `assets/pack/rp/` | Resource pack: `textures/`, `models/`, fragments `item_texture.d/`, `terrain_texture.d/`, `blocks.d/`, `sounds.d/`, `texts.d/<locale>/*.lang`. |
| `tools/build.ps1` | Builds server and/or client into `bin/<target>/Bedstones/`. |
| `tools/xmake/assemble.lua` | Post-build: copies binaries, merges lang + pack fragments. |
| `tools/textures/lib.ps1` | Pixel-art helpers (`Save-PixelArt`, `Merge-Rows`, `Shade-Color`). |
| `tools/e2e/` | Real-game harnesses (server + client); see `tools/e2e/README.md`. |

## Work as a feature unit (no shared-file edits)

Each feature unit owns its own files and registers itself. **Do not edit** `src/mod/Main.cpp`,
existing contracts or other units' files. Extend contracts compatibly (add, never change/remove)
and only when your unit really needs it; call such additions out in the PR.

Files a unit typically adds (`<unit>` = e.g. `sharestone`):
- `src/feature/<unit>/*.cpp|h`, namespace `bs::feature::<unit>`; screens in `src/client/screens/<unit>/`.
- `assets/lang/en_US/<unit>.json` + `assets/lang/zh_CN/<unit>.json` (both required).
- BP: `assets/pack/bp/blocks/<id>.json`, `items/<id>.json`, `recipes/<id>.json`, `loot_tables/blocks/<id>.json`.
- RP: `textures/blocks|items/*.png` (drawn by `tools/textures/<unit>.ps1`), `models/blocks/*.geo.json`,
  fragments `terrain_texture.d/<unit>.json`, `item_texture.d/<unit>.json`, `blocks.d/<unit>.json`,
  `sounds.d/<unit>.json`, `texts.d/{en_US,zh_CN}/<unit>.lang` (block / item names).
- Selftests in the unit's .cpp (`BS_REGISTER_SELFTEST("<unit>.<case>", ...)`), e2e scenarios in
  `tools/e2e/{server,client}/scenarios/<unit>.ps1`.

Duplicate keys in any merged fragment (lang JSON, item_texture, terrain_texture, blocks, texts,
sounds) fail the build. A translation key belongs to the unit that owns the feature; other units
reference it with `tr(key, "English fallback")` without defining it.

### Registration macros

| Macro | Header | Purpose |
| ----- | ------ | ------- |
| `BS_REGISTER_MODULE(T)` / `BS_REGISTER_MODULE_ORDERED(T, order)` | `mod/Module.h` | `T` has `install()`/`uninstall()` (+ optional `load()`, + optional `static T& get()`). |
| `BS_REGISTER_CONFIG("section", T)` | `mod/Config.h` | Reflectable struct with `int version`; file `config/<section>.json`; `bs::config::get<T>()`. Waystones' sections are owned by units: `general` (U9), `teleports` (U7), `inventoryButton` (U14), `worldGen` (U15), `client` (U17). |
| `BS_REGISTER_SUBCOMMAND(fn)` | `mod/Commands.h` | Adds overloads to `/waystones` (alias `/bedstones`). `commands::playerOf`, `commands::isAdmin`. |
| `BS_REGISTER_SELFTEST("unit.case", fn)` | `selftest/SelfTest.h` | `fn(Context&)` or `ll::coro::CoroTask<>(Context&)`. |
| `BS_REGISTER_SCREEN_ACTIONS("id", fn)` / `BS_REGISTER_SCREEN_FALLBACK("id", fn)` | `net/Screens.h` | Server side of a screen: client actions, and the form shown to players without the client mod. |
| `BS_REGISTER_CLIENT_SCREEN("id", factory)` | `client/ui/screen/Screens.h` | The native screen drawn for id (client build). |
| `BS_REGISTER_CLIENT_MESSAGE("channel", fn)` / `BS_REGISTER_SERVER_MESSAGE("channel", fn)` | `net/Messages.h` / `client/net/ClientNet.h` | Named JSON channels client -> server / server -> client. |
| `BS_REGISTER_TARGET_PROVIDER(TargetProvider{...})` | `service/TargetProviders.h` | Extra selection-list entries (feather partners, memorials, return portal). |
| `BS_REGISTER_UI_LAYER(instance)` | `client/ui/UiRoot.h` | A layer drawn over vanilla screens / HUD (e.g. the inventory button). |
| `BS_REGISTER_SCREEN_OPENER("name", fn)` | `service/ScreenOpeners.h` | Opens a screen this unit owns when another unit asks `service::openScreen(player, "name", args)` (table of names + args in the header). |

Registrations live in .cpp files linked directly into the DLL, so they are never stripped.

## Contracts in short

- **Ids** (`game/Ids.h`): `bedstones:<path>` with Waystones' paths (`waystone`, `mossy_waystone`,
  `red_sharestone`, `white_portstone`, `warp_plate`, `warp_scroll`, ...). Custom block states are
  integers: `bedstones:facing` (Java 2D value: 0 S, 1 W, 2 N, 3 E; placed blocks face the player),
  `bedstones:upper` (0/1, two-tall blocks), `bedstones:status` (warp plate). Item NBT strings:
  `ids::kNbt*` via `game::userString/setUserString`.
- **Records** (`game/Waystones.h`): every waystone-like block (waystone, sharestone, warp plate,
  warp portal, fleeting memorial) is a `core::WaystoneRecord` keyed by uid and by the position of
  its lower block. `create/find/findAt/all/ofType/move/markDirty/remove`, change listeners
  (`onChanged`, `onPlayerChanged`). Custom blocks have no block entities: per-block data lives in
  the record's `extra` JSON (key it by unit, e.g. `extra["modifiers"]`). Never keep record pointers
  across ticks; keep the uid.
- **Player data** (`Waystones::playerData(player)`): activated list, sorting index, sort mode,
  cooldowns, aliases, groups, hidden, group registry, `extra` for unit data; `activate/forget`,
  `markPlayerDirty`. Persisted per world (`bs:player:<uuid>`).
- **Service seams** (`service/Services.h`): `service::get<ITeleport>()`, `IRequirements`,
  `IPermissions`, `INameGenerator`, `ITargets`, `ISelection`, `IEditScreen`, `IArrival`. Defaults
  keep everything working; the owning unit replaces one with `service::set<I>(...)` in `install()`
  and `service::reset<I>()` in `uninstall()`. Use the seams instead of calling other units.
  Opening the selection menu = `service::get<ISelection>().open(player, source)`; warping =
  `service::get<ITeleport>().warp(player, {source, targetId})`.
- **Screen links**: screens open each other by name through `service::openScreen` (selection ->
  manage -> personal_settings -> manage_groups -> edit_group; selection header / shift-click ->
  edit -> modifiers). `openScreen` returns false when the owning unit is absent: degrade gracefully
  (hide or disable the button, `service::hasScreen(name)`).
- **Block container** (`game/BlockContainer.h`): the 5-slot shard + modifier inventory of waystones
  and warp plates, in `record.extra["container"]` (slot 0 shard, 1-4 modifiers).
- **Groups** (`service::get<IGroups>()`, ids in `service::groups`): `groups(player)` and
  `groupsOf(player, target)`; favorites are the explicit group `waystones:favorites` in
  `playerData.groups[uid]`.
- **Shared player-data keys** (`PlayerWaystoneData`): `cooldowns` = cooldown id ("waystones:inventory_button")
  -> expiry epoch ms (U8 writes); `extra["returnPortal"]` = uid of the player's last Warp Portal
  record (U5 writes, U4's Return Scroll reads); `extra["feather"]` (U6); `sortMode` /
  `sortingIndex` (U10 / U11).
- **Screens** (`net/Screens.h` + `client/ui/screen/Screens.h`): the server opens a screen id with a
  JSON model (`screens::open`), updates it (`screens::update`), receives actions
  (`BS_REGISTER_SCREEN_ACTIONS`). The client mod draws the registered native `Screen` (vanilla look
  from `widgets/Style.h`, widgets from `widgets/Widgets.h`, text boxes `widgets/TextBox.h`).
  Players without the client mod get the `BS_REGISTER_SCREEN_FALLBACK` form (ll::form). One screen
  per player; opening another replaces it. Never trust client data: re-validate every action.
  Local client-only screens: `screens::openLocal(id, model)` (e.g. a confirm dialog).
- **Strings**: `bs::tr("key", "English fallback", args...)` / `bs::trFor(player, ...)`, client
  `bs::client::ctr(...)`, `{}`/`{0}` placeholders. Prefer Waystones' own key names
  (`gui.waystones.waystone_selection.title`, `chat.waystones.*`, ...) for strings that exist there;
  `bs.<unit>.*` for Bedstones-only text. Block / item names come from RP `texts.d`
  (`tile.bedstones:<path>.name`, `item.bedstones:<path>`).
- **Feedback** (`game/Effects.h`): `actionBar` (Java's action bar; errors in "§4" dark red),
  `chat`, `sound/soundAround`, `particle/particles`.
- **Logging**: `bs::log()` (`mod/Log.h`). No per-tick logging. Log one line per user-visible action
  that e2e scenarios can wait for (e.g. `waystone activated: <name> by <player>`).
- **Selftests** (`selftest/SelfTest.h`): `ctx.player()` (SimulatedPlayer), `ctx.spawnPlayer()`,
  `ctx.testPos(dx)` (distinct `dx` per test), `ctx.createWaystone(record)`, `ctx.setBlock(pos, id,
  states)`, `ctx.cleanupBlock/cleanupWaystone`, `ctx.give`, `co_await ctx.ticks(n)`,
  `ctx.check/require/fail/info`. Screens for simulated players: `net::setCompanion(p, info)` +
  `net::setTestSink(...)`, drive them with `net::screens::simulateAction`.

## Client mod

- Every server screen has a native client screen (Java GUI look, Waystones layout from
  docs/SPEC.md §4) and a form fallback for vanilla clients.
- Client threads: render/network/posted input on the client thread; raw input on the window
  thread (`client/input/InputRouter.h`). Client-only .cpp files start with `#ifdef BS_CLIENT`.
- The host logs `companion: screen <id> shown` the first frame a screen is drawn and
  `companion: server ready` after the handshake: e2e scenarios wait for these.

## Build, test, run

In a fresh pwsh refresh PATH first (the build script does this and adds LLVM if needed):
`$env:Path=[Environment]::GetEnvironmentVariable('Path','Machine')+';'+[Environment]::GetEnvironmentVariable('Path','User')`

```powershell
pwsh tools/build.ps1 -Target all -Tests     # bin/server/Bedstones + bin/client/Bedstones + gtest
pwsh tools/build.ps1 -Target server -Tests  # one platform + unit tests
xmake build -y bs-tests; xmake run bs-tests
pwsh -NoProfile -File tools/e2e/server/run.ps1 -Scenario smoke       # BDS (4 slots, waits for a free one)
pwsh -NoProfile -File tools/e2e/client/run.ps1 -Scenario smoke       # headless client via LeviHeadlessInput
```

- `git config --global core.longpaths true` is required (long LeviLamina header paths).
- `clang-cl` must be on PATH (`C:\Program Files\LLVM\bin`).
- `xmake-requires.lock` is committed; it holds entries for both target types.
- In game: `/waystones selftest ["glob"]` (op or server console). Quote globs: `waystones selftest "sharestone.*"`.
  Output lines: `SELFTEST PASS <name>`, `SELFTEST FAIL <name>: <reason>`, `SELFTEST DONE pass=<N> fail=<M>`.
- Log contract: `Bedstones enabled` on enable; `Bedstones world ready` when a world is available.
- E2E client runs: set `$env:LEVI_E2E_SLOTS='1,2,3,4'` to take the first free game slot. Locks live in
  `%APPDATA%\levilauncher.exe\` (`bei-e2e.lock` for slot 1, `hiN-e2e.lock` for clones); the harness
  waits for them. Headless input (LeviHeadlessInput) is the default: the game runs in the
  background and never takes the real mouse/keyboard.

Deploying by hand: copy `bin/server/Bedstones` to `<BDS>/plugins/`, or `bin/client/Bedstones` to
`%APPDATA%\levilauncher.exe\versions\1.26.51.01\mods\`.

## Bedrock / LeviLamina rules (learned the hard way)

- Headers: `%LOCALAPPDATA%\.xmake\packages\l\levilamina\26.51.7\<hash>\include\{ll,mc}` (one hash per
  target type). Check them before guessing a name or signature.
- Callable: `MCAPI`, `MCFOLD`, `MCTAPI`, unmarked `virtual`. **Never** `MCNAPI`. `ll::TypedStorage`
  members are fields (`.get()`, `*`, `->`). Packets without an MCAPI constructor are created with
  `MinecraftPackets::createPacket(MinecraftPacketIds::X)` + `static_cast`; send with `pkt.sendTo(player)`.
- Hook virtuals via `&Cls::$fn` with `LL_TYPE_INSTANCE_HOOK`; `hook()` in `install()`, `unhook()` in `uninstall()`.
- In `ServerNetworkHandler::$handle` hooks `this` is the `NetEventCallback` base subobject: use
  `thisFor<NetEventCallback>()->_getServerPlayer(source, packet.mSenderSubId)`.
- **No `dynamic_cast` on game types** (no usable RTTI). A Player that is not `isClientSide()` is a `ServerPlayer`.
- Types used as command parameters must have external linkage (not in an anonymous namespace).
- `ItemStack` has no `(name, count, aux)` constructor: use `game::makeItem(id, count, aux)`.
- BDS enables mods **after** "Server started" (the world already exists at `install()`); the client
  enables mods at game start (no world yet) and fires `ServerStartedEvent` for every local world.
  Write modules that work in both orders (`game::db::onWorldReady`).
- `ll::service::getLevel()` on the client is the integrated server's level (null outside a world).
- Behavior packs in `mods/<name>/behavior_packs/` are added to every world by LeviLamina; resource
  packs are added to the client's global stack. On BDS `PackDeployer` adds the RP to the world.
- Custom blocks cannot have block entities: keep per-block state in block states + the waystone record.
- A cancelled `PlayerInteractBlockEvent` rolls back block changes made during it: defer world edits
  to the next tick (`ll::coro` / `ServerLevelTickEvent`).
- Don't keep `Player*` across ticks; store `getOrCreateUniqueID()` / the UUID and look it up again.

## Conventions

- C++20, `.clang-format` (LLVM-based, 4 spaces, 120 columns). Namespaces follow directories
  (`bs::core`, `bs::game`, `bs::service`, `bs::net`, `bs::feature::<unit>`, `bs::client::...`).
- English in code and comments; user-facing text through `tr`/`trFor`/`ctr` + lang fragments (en_US and zh_CN).
- Every `src/core` change comes with gtest coverage in `tests/`; every feature comes with selftests
  and an e2e scenario.
- Commit in small, buildable steps. Both platforms must build (`pwsh tools/build.ps1 -Target all`).
- Commit / PR titles: `U<n>: <summary>` (unit id from the plan), PR into `main`.

## Out of scope

JourneyMap / Dynmap / BlueMap / Jade integrations (no Bedrock equivalent) and the
`waystones:waystone_activated` statistic (Bedrock has no custom statistics).
