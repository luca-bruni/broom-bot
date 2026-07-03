# broom-bot Architecture

General-purpose Discord bot in C++20 using [D++ (DPP)](https://github.com/brainboxdotcc/DPP) v10.1.4,
buildable from source on macOS, Linux, and Windows. MIT licensed.

## Layout

```
src/
  main.cpp            Entry point: load config, construct cluster, wire registry, start
  core/               Bot-agnostic infrastructure (no feature logic)
    config.hpp/.cpp   .env + environment config loading
    command.hpp       Command interface
    registry.hpp/.cpp CommandRegistry: bulk-registers commands with Discord, dispatches events
  commands/           One .cpp per command (feature modules)
    ping.cpp
    coinflip.cpp
    all_commands.cpp  Explicit factory list of every command
external/DPP          DPP pinned as git submodule
CMakeLists.txt        Dual-mode build (see Build)
.env                  Secrets, gitignored (.env.example documents required keys)
```

## Command system

- `Command` is an interface: `std::string name()`,
  `dpp::slashcommand definition(dpp::snowflake app_id)`, and
  `void handle(const dpp::slashcommand_t& event)`.
- `commands/all_commands.cpp` returns an explicit `std::vector<std::unique_ptr<Command>>`.
  **Deliberately not static self-registration**: static registrar objects are dead-stripped
  by MSVC and have fragile init order; an explicit list is deterministic and cross-platform.
- `CommandRegistry` owns the commands, maps name → command:
  - `on_ready` (guarded by `dpp::run_once`): registers all definitions with Discord in one
    bulk call.
  - `on_slashcommand`: map lookup → `handle()`. Unknown command → logged, ignored.
- **Adding a command** = one new `.cpp` in `commands/` + one line in `all_commands.cpp`
  + one line in `CMakeLists.txt`. Nothing else changes.

## Config

- `core/config` parses `.env` (KEY=VALUE, `#` comments) into a map, then overlays real
  environment variables (env wins, enabling container/CI overrides).
- **Never calls `setenv`** — it doesn't exist on MSVC. Config is passed by value, not
  through the process environment. (The pre-refactor `main.cpp` violated this.)
- Keys:
  - `BOT_TOKEN` (required) — Discord bot token.
  - `DEV_GUILD_ID` (optional) — when set, commands register guild-scoped (instant
    propagation, for development). When unset, commands register globally (production;
    Discord may take up to ~1 h to propagate).

## Build

CMake ≥ 3.20, C++20. Build type defaults to Release on single-config generators.
Two modes via `BB_BUILD_DPP` (default `OFF`):

- **`-DBB_BUILD_DPP=ON`** — builds the DPP submodule and installs it to
  `external/DPP/install`. Use on first build or after bumping the submodule.
  Requires `git submodule update --init --recursive` first.
- **`OFF`** — links the pre-built DPP from `external/DPP/install` via
  `find_package(dpp CONFIG)` (DPP installs a CMake package config). An external DPP
  install can be substituted with `-DCMAKE_PREFIX_PATH=...`.
  **Quirk**: DPP's exported target omits `INTERFACE_INCLUDE_DIRECTORIES`; our
  CMakeLists patches the include path onto `dpp::dpp` after `find_package`.

Both modes link the same `dpp::dpp` target (ON mode aliases the in-tree target).
On Windows, post-build and install steps copy `dpp.dll` plus DPP's bundled deps
(`external/DPP/win32/bin`: OpenSSL, zlib, opus) next to the executable.

Typical flow:

```sh
cmake -B build -DBB_BUILD_DPP=ON   # first time
cmake --build build
./build/broom_bot                   # run from repo root (reads ./.env)
```

DPP's own dependencies (OpenSSL, zlib) must be available on the system; see DPP docs
per platform.

## CI

`.github/workflows/build.yml` builds all three OSes (ubuntu/windows/macos) on every
push and PR to master, then smoke-tests that the binary starts and exits cleanly on
missing config. `external/DPP/install` is cached keyed on the pinned submodule SHA +
runner image version: **bumping the DPP submodule automatically triggers one DPP
rebuild per platform**, after which builds take ~1–2 min again. To bump DPP:
`cd external/DPP && git fetch && git checkout <tag>`, then commit the gitlink; CI
validates the bump across platforms before merge.

## Conventions

- Explicit source lists in CMake (no globbing).
- `core/` must not depend on `commands/`; commands depend only on `core/` and DPP.
- Logging goes through `bot.on_log` (wired to `dpp::utility::cout_logger` in main),
  not raw stdout.
- Secrets only in `.env` / environment — never committed, never logged.
- Blocking work inside handlers is forbidden; use DPP's async/coroutine or callback APIs
  (handlers run on the cluster's event threads).

## Status

Structure implemented and merged to master. Verified on macOS x86_64: both CMake
modes configure and build, and `/ping` + `/coinflip` answered end-to-end against a
live dev guild. Windows/Linux verified by the CI matrix on every PR.
