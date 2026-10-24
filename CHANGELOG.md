# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- A logo (`docs/logo.png`), shown on [LeviMC Mods](https://levimc.org/mods/) through `avatar_url` in `tooth.json`.
- A player-facing README, and screenshots and a video for the LeviMC Mods gallery (`docs/showcase/`).

## [0.1.0] - 2026-10-24

First release. Bedstones brings the behaviour of the Java mod Waystones 21.1.46 to Minecraft Bedrock Edition 1.26.51 with LeviLamina 26.51.x. It runs on the dedicated server and on the client.

### Added

- **Waystones** in 10 styles: waystone, mossy, sandy, deepslate, black, ender, nether, purpur, prismarine and muddy.
  - Activate one by right-clicking it, then warp between activated waystones.
  - Name a waystone when you place it. Its visibility can be activation, team or global.
  - Owner and edit permissions follow Waystones' rules.
  - Glowing runes and particles appear only for players who activated the waystone.
- **Sharestones** in 15 colours. Each one links to every sharestone of its colour, with no activation needed.
- **Portstones** in 16 colours. They open a selection menu but are not destinations themselves.
- **Warp plates** with dormant, attuned, crumbling and deepslate shards.
- **Warp portals**, opened by the portal scroll.
- **Fleeting memorials**, left by the Epitaph.
- **Warp modifiers**: a 4-slot modifier container on waystones and warp plates, with every Waystones modifier.
- **Items:**
  - warp stone;
  - warp, return, blank, bound and portal scrolls;
  - warp dust;
  - Twinbound Feather;
  - Epitaph.

  Recipes, durability and charge times match Waystones.
- **Warp requirements**: XP costs and cooldowns through Waystones' rule language (`warpRequirements`), with the same defaults. The selection list shows each destination's cost.
- **Teleport rules**: pets, leashed mobs and the entity deny list.
- **Screens**, drawn natively with the Java look by the client mod:
  - waystone selection, with sorting, groups and favorites;
  - Manage Waystones;
  - Personal Settings;
  - Manage Groups and Edit Group;
  - Edit Waystone;
  - Warp Modifiers;
  - the inventory button.
- **Form screens** for players without the client mod, so vanilla clients can join a Bedstones server and use every feature.
- **World generation**:
  - wild waystones, styled by biome;
  - village waystones;
  - generated names (`{MrPork}` template and presets).
- **Commands**: `/waystones activate | forget | place | count | list | gui | twinbound | cooldown | reload`.
- **Configuration** with Waystones' option names, as one JSON file per section in the mod's `config/` folder.
- **Translations**: English and Simplified Chinese.
- **Resource pack on servers**: a dedicated server adds the behavior pack and resource pack to the world, and joining players download the textures.

[Unreleased]: https://github.com/LiteLDev/Bedstones/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/LiteLDev/Bedstones/releases/tag/v0.1.0
