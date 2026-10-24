# Contributing to Bedstones

Thanks for helping! Bug reports, ideas and pull requests are all welcome.

## Reporting bugs and requesting features

Use the [issue templates](https://github.com/LiteLDev/Bedstones/issues/new/choose). For bugs, include:

- the Bedstones version;
- the LeviLamina version;
- the Minecraft version;
- whether it happens on a dedicated server, in a local world, or with a vanilla client;
- steps to reproduce;
- the relevant part of the server console or `logs/latest.log`.

## Development setup

See [Building](README.md#building) in the README. [AGENTS.md](AGENTS.md) is the developer guide. It covers the layout of the code, the feature units and the contracts between them, the registration macros and the rules for working with Bedrock internals. Please read it before changing anything under `src/`. The behaviour reference is [docs/SPEC.md](docs/SPEC.md).

## Pull requests

1. Fork the repository and create a branch from `main`.
2. Keep each pull request focused on one change. Describe what it does and how you tested it.
3. Follow the existing style: C++20, the provided `.clang-format` and `.editorconfig`, English in code and comments.
4. User-facing text goes through `bs::tr` / `bs::trFor` / `bs::client::ctr` with entries in **both** `assets/lang/en_US/` and `assets/lang/zh_CN/`.
5. Logic in `src/core` must stay free of game headers and come with unit tests in `tests/`:
   `xmake build -y bs-tests; xmake run bs-tests`.
6. Gameplay changes come with selftests (`BS_REGISTER_SELFTEST`) and an end-to-end scenario in [tools/e2e](tools/e2e/README.md). Run the relevant scenarios, and `tools/e2e/server/run.ps1 -Scenario full` for anything that touches shared code.
7. Both platforms must build: `pwsh tools/build.ps1 -Target all -Tests`. CI must pass before a pull request is merged.
8. Never copy code or assets from the Java Waystones repository. It is a behaviour reference only.

Commit messages use the imperative mood ("Add warp plate cooldown", not "Added …"). Update [CHANGELOG.md](CHANGELOG.md) under *Unreleased* for user-visible changes.

## Releasing (maintainers)

1. In [CHANGELOG.md](CHANGELOG.md), move the *Unreleased* entries under a new `## [x.y.z] - YYYY-MM-DD` section and update the links at the bottom. Set `version` in [tooth.json](tooth.json) to `x.y.z`.
2. Merge that into `main`. Then run every server and client e2e scenario against `main`.
3. Tag the merge commit `vx.y.z` and push the tag. The *Release* workflow:
   - builds both platforms and runs the unit tests;
   - checks that the mod manifest and `tooth.json` carry the tag's version;
   - publishes `bedstones-server-windows-x64.zip` and `bedstones-client-windows-x64.zip`, with that version's CHANGELOG section as the release notes.

## License

By contributing you agree that your contributions are licensed under the [LGPL-3.0](COPYING.LESSER), the license of this project.
