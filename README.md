# Bedstones

[![Build](https://github.com/LiteLDev/Bedstones/actions/workflows/build.yml/badge.svg)](https://github.com/LiteLDev/Bedstones/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/LiteLDev/Bedstones)](https://github.com/LiteLDev/Bedstones/releases/latest)
[![License](https://img.shields.io/github/license/LiteLDev/Bedstones)](COPYING.LESSER)

Waystones for Minecraft Bedrock Edition: a [LeviLamina](https://github.com/LiteLDev/LeviLamina) mod that brings the behaviour of the Java mod **Waystones** (BlayTheNinth, version 21.1.46) to Bedrock. It runs on the dedicated server (BDS) and on the client (local worlds), from one code base.

Bedstones copies no code or assets from Waystones. Its textures are original pixel art, and [docs/SPEC.md](docs/SPEC.md) describes the behaviour it reproduces.

## Table of Contents

- [Features](#features)
- [Install](#install)
- [Usage](#usage)
  - [Getting started](#getting-started)
  - [Blocks](#blocks)
  - [Items](#items)
  - [Commands](#commands)
  - [Configuration](#configuration)
- [Building](#building)
- [Maintainers](#maintainers)
- [Contributing](#contributing)
- [License](#license)

## Features

- **Blocks:**
  - waystones in 10 styles;
  - sharestones in 15 colours;
  - portstones in 16 colours;
  - warp plates;
  - warp portals and fleeting memorials.
- **Items:**
  - the warp stone;
  - warp, return, bound and portal scrolls;
  - shards;
  - the Twinbound Feather and the Epitaph.
- **Waystones' screens**, drawn natively by the client mod:
  - destination selection;
  - Manage Waystones;
  - personal settings;
  - groups;
  - edit and warp modifiers;
  - the inventory button.

  Players without the client mod get form-based versions of the same screens, so a server with Bedstones accepts vanilla clients too.
- **Warp costs and cooldowns** through Waystones' warp requirement rules (XP costs by distance, the inventory button cooldown).
- **Wild and village waystones** in newly generated chunks.
- The `/waystones` admin commands.
- English and Simplified Chinese.

## Install

Requirements:

- Minecraft Bedrock Edition **1.26.51** for Windows (x64).
- LeviLamina **26.51.x**, on the server or on the client (for example installed through [LeviLauncher](https://github.com/LiteLDev/LeviLauncher)).

With [lip](https://github.com/futrime/lip):

```sh
lip install github.com/LiteLDev/Bedstones           # dedicated server
lip install github.com/LiteLDev/Bedstones#client    # client
```

Manually: download the zip from the [latest release](https://github.com/LiteLDev/Bedstones/releases/latest) and extract it so that the `Bedstones/` folder ends up next to `LeviLamina/`:

| Side | Zip | Folder |
| --- | --- | --- |
| Dedicated server | `bedstones-server-windows-x64.zip` | `<BDS>/plugins/Bedstones/` |
| Client | `bedstones-client-windows-x64.zip` | the game's `mods/Bedstones/` |

- **Server:** Bedstones adds its behavior pack and resource pack to the world, so players download the textures when they join. Players can join with or without the client mod.
- **Client:** the client mod plays local worlds by itself. On a server it needs Bedstones on the server too, and then draws the native screens instead of forms.

## Usage

### Getting started

1. Craft a **warp stone**: amethyst shards, ender pearls and an emerald.
2. Craft a **waystone**: stone bricks around the warp stone, on a row of obsidian.
3. Place it and give it a name. Placing it also activates it for you.
4. Right-click another player's waystone, or a wild or village one, to **activate** it.
5. Right-click an activated waystone, or use a warp stone or warp scroll, to open the **selection menu** and warp to any waystone you have activated.

Warping costs XP points, scaled by distance. Changing dimension costs 27, which is also the cap. Scrolls, warp plates and global waystones are free. Shift + right-click a waystone to edit its name and visibility (activation, team or global), or to open its warp modifiers.

### Blocks

| Block | What it does |
| --- | --- |
| Waystone (10 styles) | A teleport destination. Activate it once, then warp to it from any other waystone, warp stone or scroll. |
| Sharestone (15 colours) | Every sharestone of one colour links to all the others on the server, with no activation needed. |
| Portstone (16 colours) | Opens a selection menu but is not a destination itself. The white one lists your waystones, and a coloured one lists the sharestones of its colour. |
| Warp Plate | Put a dormant shard in it to attune the shard to this plate. A plate holding that attuned shard teleports whatever stands on it back here. |
| Warp Portal | Opened by a portal scroll. It teleports entities to the chosen destination for 5 minutes. |
| Fleeting Memorial | Placed where you die while carrying an Epitaph. You can warp back to it once. |

Waystones and warp plates also hold up to 4 **warp modifiers** that affect entities arriving there, such as blaze powder (fire), feathers (slow falling) or a redstone torch (turns a plate off while powered).

### Items

| Item | What it does |
| --- | --- |
| Warp Stone | Hold use to open the selection menu. It has 128 durability. |
| Warp Scroll | Like the warp stone, but consumed on use. |
| Return Scroll | Takes you back to your last warp portal, or to the nearest activated waystone. |
| Blank Scroll / Bound Scroll | Use a blank scroll on a waystone to get a bound scroll that warps straight to it. |
| Portal Scroll | Opens a warp portal to the destination you choose. |
| Warp Dust, Dormant Shard, Deepslate Shard | Warp plate ingredients. A deepslate shard locks a plate. |
| Twinbound Feather | Two players link their feathers. Each one can then warp to the other. |
| Epitaph | Leaves a fleeting memorial where you die. |

The inventory button, when the server enables it, warps you to your nearest waystone from the inventory screen, with a 300-second cooldown.

### Commands

`/waystones` (alias `/bedstones`). Every subcommand except `inventorybutton` and `companion` needs operator permission.

| Command | Effect |
| --- | --- |
| `activate <players> <pos>` | Activate the waystone at `pos` for the players |
| `forget <players> <pos>` / `forget <players> all` | Deactivate one or all waystones |
| `place <pos> <style> <name>` | Place a named waystone |
| `count <player>` / `list <player> [all]` | Show a player's waystones |
| `gui <player>` | Open a player's selection menu as an admin |
| `twinbound <first> <second>` | Link two players' Twinbound Feathers |
| `cooldown <players> reset all \| reset <id> \| add <id> <seconds> \| set <id> <seconds>` | Manage warp cooldowns |
| `reload` | Reload the configuration and translations |
| `inventorybutton` | Use the inventory button (for players without the client mod) |
| `companion` | Show who runs the client mod |

### Configuration

The configuration is stored as one JSON file per section in `config/` inside the mod folder (`plugins/Bedstones/config/` on a server). It uses Waystones' option names.

| File | Options |
| --- | --- |
| `general.json` | `restrictedWaystones`, `defaultVisibility`, `allowEveryoneToManageGlobalWaystones`, name generation (`nameGenerationMode`, `nameGenerationTemplate`, `nameGenerationPresets`) |
| `teleports.json` | `transportPets`, `transportLeashed`, `entityDenyList` |
| `requirements.json` | `enableCosts`, `enableCooldowns`, `warpRequirements` |
| `items.json` | `warpStoneUseTime`, `scrollUseTime`, `enableDurability` |
| `warpPlate.json` | `warpPlateUseTime`, `warpPlateCooldownTime` |
| `modifiers.json` | `enableModifiers` |
| `inventoryButton.json` | `inventoryButton`: empty (off, the default), `NEAREST`, `ANY` or a waystone name; the button position |
| `worldGen.json` | `wildWaystoneStyle`, `chunksBetweenWildWaystones` (0 turns wild waystones off), `wildWaystonesDimensionAllowList` / `DenyList`, `spawnInVillages` |
| `client.json` | `disableTextGlow` (client mod) |
| `mod.json` | `language`, `usePlayerLocale`, `requireResourcePack` |

Run `/waystones reload` after editing a file.

## Building

You need Windows x64, [xmake](https://xmake.io), Visual Studio Build Tools and LLVM (`clang-cl`).

```powershell
git config --global core.longpaths true
pwsh tools/build.ps1 -Target all -Tests     # bin/server/Bedstones/ and bin/client/Bedstones/, plus unit tests
```

The mod version comes from the latest `v*` git tag.

- [AGENTS.md](AGENTS.md) describes the layout, the feature units and the contracts between them, and the rules for working with Bedrock internals.
- [tools/e2e](tools/e2e/README.md) holds the end-to-end harnesses that run the real server and client, headless through [LeviHeadlessInput](https://github.com/LiteLDev/LeviHeadlessInput).

## Maintainers

[@futrime (Zijian Zhang)](https://github.com/futrime)

## Contributing

PRs and issues are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) and follow the [Code of Conduct](CODE_OF_CONDUCT.md). Report security issues as described in [SECURITY.md](SECURITY.md).

## License

LGPL-3.0 © 2026 Zijian Zhang

Bedstones is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License version 3, the same license as LeviLamina. See [COPYING.LESSER](COPYING.LESSER) and [COPYING](COPYING).

Bedstones is not affiliated with Mojang Studios, Microsoft, or BlayTheNinth and the Waystones project.
