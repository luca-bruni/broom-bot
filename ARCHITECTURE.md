# broom-bot Architecture

General-purpose Discord bot in C++20 using [D++ (DPP)](https://github.com/brainboxdotcc/DPP) v10.1.4,
buildable from source on macOS, Linux, and Windows. MIT licensed.

The full command list lives in the [README](README.md); this document describes
how the project is put together. For the message-cache / bulk-operations design
see [docs/bulk-message-processing.md](docs/bulk-message-processing.md).

## Layout

```
src/
  main.cpp            Entry point: load config, open DB + run migrations, build
                      cluster, wire registry + job runner, start.
  core/               Bot-agnostic infrastructure (no feature logic)
    config.hpp/.cpp   .env + environment config loading
    command.hpp       Command interface
    registry.hpp/.cpp CommandRegistry: bulk-registers commands, routes events
    services.hpp      Services{ JobRunner&, Db&, catalog, uptime } injected
                      into commands
    catalog.hpp       Command name/description snapshot for /help and /stats
    db.hpp/.cpp       SQLite (vendored) RAII wrapper + migrations
    jobs.hpp/.cpp     JobRunner/JobContext: background job subsystem
    rest_await.hpp    await_rest<T>: cancellation-aware blocking REST call
                      for job worker code
    duration.hpp      parse_duration_seconds ("30d", "1w2d", …)
    timeparse.hpp     date/snowflake/uptime parsing + formatting
    rng.hpp           Shared thread-safe RNG helper
  commands/           One .cpp per command (feature modules); listed in
                      all_commands.cpp. See README for the full set.
                      Shared command helpers live here as headers: embeds.hpp
                      (kEmbedColor, created_relative, require_guild),
                      options.hpp (typed subcommand option lookup), and the
                      DPP-free testable logic (info_format.hpp,
                      purge_filter.hpp, remind_rules.hpp,
                      eightball_answers.hpp).
external/DPP          DPP pinned as a git submodule
external/sqlite       SQLite amalgamation, vendored (not a submodule)
external/doctest      doctest single header, vendored (unit-test framework)
tests/                doctest unit tests (pure logic)
CMakeLists.txt        Dual-mode build (see Build)
docs/                 Design docs
.env                  Secrets, gitignored (.env.sample documents required keys)
data/                 Runtime SQLite database (gitignored; DATA_DIR)
```

## Command system

- `Command` is an interface: `std::string name()`,
  `dpp::slashcommand definition(dpp::snowflake app_id)`,
  `void handle(const dpp::slashcommand_t&)`, and optional
  `void handle_button(const dpp::button_click_t&)`.
- Options are declared in `definition()` (`.add_option(...)`). Simple ones are
  read via `event.get_parameter("name")`; **subcommands** are read from
  `event.command.get_command_interaction().options[0]` and its nested
  `.options` (see `commands/purge.cpp`, `commands/jobs.cpp`).
- **Component custom_id convention**: `"<command-name>:<action>"`. The registry
  routes `on_button_click` to the command whose `name()` matches the prefix
  before `:` (see `commands/coinflip.cpp`). Other subsystems may attach their
  own `on_button_click` handlers for prefixes the registry doesn't own (the job
  runner claims `job:cancel:<id>`).
- **Dispatch is exception-guarded**: a handler that throws is logged and
  answered with an ephemeral error instead of unwinding DPP's event thread
  (see `guarded()` in registry.cpp; the job-cancel button handler does the
  same).
- `all_commands(Services&)` returns an explicit
  `std::vector<std::unique_ptr<Command>>`. **Deliberately not static
  self-registration**: static registrars are dead-stripped by MSVC and have
  fragile init order; an explicit list is deterministic and cross-platform.
- Commands that need infrastructure (background jobs, the database) take
  `Services&` in their constructor; the rest ignore it.
- **Adding a plain command** = one `.cpp` in `commands/` + one line in
  `all_commands.cpp` + one line in `CMakeLists.txt`.
- **Adding a job-backed command** additionally: register its job kind(s) with
  the `JobRunner` in `main`, and append any new tables to the migration list
  (see Persistence).

## Persistence (SQLite)

- SQLite is **vendored** as the amalgamation in `external/sqlite/` (its official
  distribution format; building from the Fossil source would need a TCL
  toolchain). Compiled as a small static library; `LANGUAGES C CXX`.
- `core/db` wraps it: opened in serialized (`FULLMUTEX`) mode so one `Db` is
  safe across the main and worker threads; WAL journaling; foreign keys on;
  prepared-statement helper; errors become `std::runtime_error`. The header only
  forward-declares `sqlite3`, so `sqlite3.h` is included in exactly one TU.
- **Migrations** are a single global, append-only list assembled in `main`
  (`job_schema()` + each feature's schema), applied by `Db::migrate()` which
  tracks `PRAGMA user_version`. NEVER reorder or insert in the middle — append
  new steps at the end, even core ones.
- Database path: `DATA_DIR` (default `./data/`, gitignored). It holds the job
  queue, per-guild settings, and feature tables. It lives only on the host
  running the bot; back it up by copying the file.

## Background jobs

Long-running work (history scans, bulk deletes) can take hours, far past the
~3s slash-command ack window, so it runs as persisted background jobs.

- `JobRunner` owns one worker thread and runs one job at a time, **at most one
  active job per guild** (`enqueue` rejects if the guild is busy). Job kinds are
  registered by name (`register_kind`) by the commands that use them.
- `JobContext` (handed to a job function) provides: `cancelled()`, throttled
  (~3s) progress-message edits, and per-channel cursor save/load for resume.
- **Resumable + crash-safe**: pagination cursors are the checkpoints; jobs left
  `running` at startup are re-queued, so job functions must be cursor-resumable
  and their side effects idempotent (e.g. deletion treats 404 as done).
- **Cancellation**: `request_cancel(job_id)` signals a mutex-guarded set checked
  by `cancelled()`; shared by the per-message Cancel button (`job:cancel:<id>`,
  starter-only) and the `/jobs cancel` command (any Manage-Messages mod).
- **REST from the worker**: DPP calls are async; job code blocks on them via
  `await_rest<T>` (`core/rest_await.hpp`), which polls a `shared_ptr<promise>`
  so abandoning the wait on cancellation can't dangle the callback and returns
  nullopt when the job is cancelled mid-wait. Progress edits are throttled to
  protect the rate-limit budget.

Not everything timed belongs in the JobRunner: jobs are exclusive per guild,
so a reminder queued for tomorrow would block `/purge` (and a running purge
would delay deliveries). `/remind` instead uses `ReminderService`
(`commands/remind.hpp`) — its own polling thread (~5s) over a `reminders`
table, fire-and-forget delivery, at-most-once semantics (rows are marked sent
before the REST call, so a crash drops rather than duplicates).

## Config

- `core/config` parses `.env` (KEY=VALUE, `#` comments) into a map, then overlays
  real environment variables (env wins, enabling container/CI overrides).
- **Never calls `setenv`** — it doesn't exist on MSVC. Config is passed by value.
- Keys: `BOT_TOKEN` (required); `DEV_GUILD_ID` (optional — guild-scoped instant
  command registration for development, else global ~1h propagation);
  `DATA_DIR` (optional — SQLite location, default `./data/`).

## Build

CMake ≥ 3.20, C++20. Build type defaults to Release on single-config generators.
`BB_BUILD_BOT=OFF` skips the bot executable and the entire DPP dependency,
building only the DPP-free unit tests (used by the sanitizer CI job).
Two modes via `BB_BUILD_DPP` (default `OFF`):

- **`-DBB_BUILD_DPP=ON`** — builds the DPP submodule and installs it to
  `external/DPP/install`. Use on first build or after bumping the submodule.
  Requires `git submodule update --init --recursive` first.
- **`OFF`** — links the pre-built DPP via `find_package(dpp CONFIG)`. An external
  DPP install can be substituted with `-DCMAKE_PREFIX_PATH=...`.
  **Quirk**: DPP's exported target omits `INTERFACE_INCLUDE_DIRECTORIES`; our
  CMakeLists patches the include path onto `dpp::dpp` (handling Windows's
  versioned `include/dpp-X.Y` layout).
  **Quirk (Windows)**: `OPENSSL_ROOT_DIR` is pinned to DPP's bundled OpenSSL 1.1
  SDK (`external/DPP/win32`) so `find_package(OpenSSL)` — invoked transitively by
  DPP's package config — can't latch onto an unrelated OpenSSL on PATH (e.g.
  PostgreSQL's 3.x), which would be ABI-incompatible with `dpp.dll` and the
  `libssl-1_1`/`libcrypto-1_1` DLLs copied next to the exe.

Both modes link the same `dpp::dpp` target. On Windows, post-build/install steps
copy `dpp.dll` + DPP's bundled deps (`external/DPP/win32/bin`) next to the exe.
DPP's own deps (OpenSSL, zlib) must be present; see DPP docs per platform.

```sh
cmake -B build -DBB_BUILD_DPP=ON   # first time
cmake --build build
./build/broom_bot                   # run from repo root (reads ./.env)
```

## Tests

Vendored **doctest** (`external/doctest/`, header-only). `tests/` cover the pure,
decision-making logic — parsers (`parse_duration_seconds`, `parse_date_ms`,
`ms_to_snowflake`) and filter predicates (`message_matches` via the DPP-free
`MessageView`). The pattern: extract a command's pure logic into a testable
header so it can be exercised without a live Discord connection; the thin
DPP glue (REST calls firing, gateway routing) is left to the CI smoke test and
manual testing. **Every new command adds tests** — build with `broom_tests`
(on by default; `-DBB_BUILD_TESTS=OFF` to skip).

## CI

`.github/workflows/build.yml` builds all three OSes on every push/PR to master
(skipping pure-docs changes via `paths-ignore`), runs the doctest suite, then
smoke-tests that the binary starts and exits cleanly on missing config. Two
extra jobs run alongside the matrix: `sanitize` (the unit tests under
ASan+UBSan via `BB_BUILD_BOT=OFF`, no DPP needed) and `format` (clang-format
check against the committed `.clang-format`).
`external/DPP/install` is cached keyed on the pinned submodule SHA + runner image
version: bumping the submodule triggers one DPP rebuild per platform, then builds
take ~1–2 min. Master is branch-protected (all three checks required to merge;
admin can still push docs directly).

## Conventions

- Formatting is enforced: `.clang-format` at the repo root, checked by the CI
  `format` job. Reformat with
  `find src tests -name '*.cpp' -o -name '*.hpp' | xargs clang-format -i`.
- Explicit source lists in CMake (no globbing).
- `core/` must not depend on `commands/`; commands depend only on `core/`, DPP,
  and (if job-backed) the vendored SQLite via `core/db`.
- Logging goes through `bot.on_log` (`dpp::utility::cout_logger`), not stdout.
- Secrets only in `.env` / environment — never committed, never logged.
- Handlers run on the cluster's event threads: no blocking work — defer to a
  background job or DPP's async APIs.
- Keep this document current: fold ARCHITECTURE.md updates into the feature PR
  that changes a subsystem.

## Status

Structure implemented and merged to master. Core infrastructure (config,
command registry, SQLite persistence, background jobs) plus utility and
moderation commands verified end-to-end against a live dev guild; Windows/Linux
verified by the CI matrix on every PR.
