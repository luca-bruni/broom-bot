# broom-bot

[![build](https://github.com/luca-bruni/broom-bot/actions/workflows/build.yml/badge.svg)](https://github.com/luca-bruni/broom-bot/actions/workflows/build.yml)

A general-purpose Discord bot in C++20, built on [D++ (DPP)](https://github.com/brainboxdotcc/DPP).
Builds from source on macOS, Linux, and Windows - verified by CI on every push.

See [ARCHITECTURE.md](ARCHITECTURE.md) for how the project is structured and how to
add commands.

## Requirements

- CMake ≥ 3.20 and a C++20 compiler (MSVC, clang, or gcc)
- OpenSSL and zlib development headers (DPP dependencies)
  - macOS: `brew install openssl`
  - Debian/Ubuntu: `apt install libssl-dev zlib1g-dev`
  - Windows: bundled with DPP, nothing to install

Prebuilt binaries for all three platforms are attached to
[GitHub Releases](https://github.com/luca-bruni/broom-bot/releases) (created
from `v*` tags).

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

**Fun**

| Command | Description |
|---|---|
| `/help` | List every command |
| `/about` | Version, library, uptime, source |
| `/stats` | Server/command counts, latency, uptime |
| `/ping` | Pong + REST latency |
| `/coinflip` | Flip a coin, with a "Flip again" button |
| `/roll [dice]` | Roll dice, e.g. `/roll dice:2d6` |
| `/choose options` | Pick one, e.g. `/choose options:pizza \| sushi` |
| `/8ball question` | Ask the magic 8-ball |

**Utility**

| Command | Description |
|---|---|
| `/remind set in message` | Set a reminder, e.g. `in:2h` - pings you in the channel when due |
| `/remind list` | List your pending reminders |
| `/remind cancel id` | Cancel one of your reminders |
| `/schedule message …` | Post a message as the bot at a future time (Manage Server) |
| `/schedule event …` | Create a Discord scheduled event (Manage Server) |

**Info**

| Command | Description |
|---|---|
| `/userinfo [user]` | Account + membership info |
| `/serverinfo` | Server stats |
| `/avatar [user]` | Full-size avatar |
| `/roleinfo role` | Role details (color, position, flags) |
| `/channelinfo channel` | Channel details (type, slowmode, topic) |
| `/emojiinfo emoji` | Custom emoji details + CDN URL |

**Moderation** (require Manage Messages)

| Command | Description |
|---|---|
| `/purge channel\|guild …` | Bulk-delete by keywords, author, pattern, `has`, `bots_only`, date range / `older_than`; dry-run → confirm, with Export |
| `/jobs list\|cancel` | View or cancel background jobs by ID |

## License

[MIT](LICENSE)
