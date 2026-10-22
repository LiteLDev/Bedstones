# Bedstones

A Bedrock Edition port of the Java mod **Waystones** (BlayTheNinth), version 21.1.46, for
[LeviLamina](https://github.com/LiteLDev/LeviLamina). It runs on the dedicated server (BDS) and
on the client (local worlds through LeviLauncher), from one code base.

- Waystones (10 styles), sharestones, portstones, warp plates, warp portals and fleeting memorials.
- Warp stone, warp / return / bound / portal scrolls, shards, Twinbound Feather and Epitaph.
- The Waystones screens drawn natively by the client mod: destination selection, manage, personal
  settings, groups, edit and warp modifiers, plus the inventory button. Players without the
  client mod get form-based versions of the same screens.
- XP costs and cooldowns through Waystones' warp requirement rules, wild and village waystones,
  and the `/waystones` admin commands.

The behaviour reference is [docs/SPEC.md](docs/SPEC.md). Bedstones copies no code or assets from
Waystones; textures are original pixel art.

## Requirements

- Minecraft Bedrock 1.26.51 with LeviLamina 26.51.7 (client through LeviLauncher, or BDS).

## Install

- BDS: copy `bin/server/Bedstones` into `<BDS>/plugins/`.
- Client: copy `bin/client/Bedstones` into
  `%APPDATA%\levilauncher.exe\versions\1.26.51.01\mods\`.

## Build

```powershell
pwsh tools/build.ps1 -Target all -Tests
```

Needs xmake, Visual Studio 2022+ build tools and LLVM (clang-cl) on PATH. See [AGENTS.md](AGENTS.md)
for the layout, the contracts between feature units, and the end-to-end test harnesses
(`tools/e2e/`, driven headlessly with LeviHeadlessInput).
