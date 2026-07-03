# broom-bot

[![build](https://github.com/luca-bruni/broom-bot/actions/workflows/build.yml/badge.svg)](https://github.com/luca-bruni/broom-bot/actions/workflows/build.yml)

A general-purpose Discord bot in C++20, built on [D++ (DPP)](https://github.com/brainboxdotcc/DPP).
Builds from source on macOS, Linux, and Windows — verified by CI on every push.

See [architecture.md](architecture.md) for how the project is structured and how to
add commands.

## Requirements

- CMake ≥ 3.20 and a C++20 compiler (MSVC, clang, or gcc)
- OpenSSL and zlib development headers (DPP dependencies)
  - macOS: `brew install openssl`
  - Debian/Ubuntu: `apt install libssl-dev zlib1g-dev`
  - Windows: bundled with DPP, nothing to install

## Building

```sh
git clone --recurse-submodules https://github.com/luca-bruni/broom-bot.git
cd broom-bot

# First build: compile the DPP submodule too (slow, one-time)
cmake -B build -DBB_BUILD_DPP=ON
cmake --build build --parallel 4

# Later builds: reuse the DPP install from the first build (fast)
cmake -B build -DBB_BUILD_DPP=OFF
cmake --build build --parallel 4
```

## Running

```sh
cp .env.sample .env   # then put your bot token in .env
./build/broom_bot     # Windows: build\Release\broom_bot.exe
```

Set `DEV_GUILD_ID` in `.env` during development so slash commands appear in your
test server instantly instead of propagating globally (~1 h).

## Commands

| Command | Description |
|---|---|
| `/ping` | Ping pong! |
| `/coinflip` | Flip a coin, with a "Flip again" button |
| `/roll [dice]` | Roll dice, e.g. `/roll dice:2d6` |

## License

[MIT](LICENSE)
