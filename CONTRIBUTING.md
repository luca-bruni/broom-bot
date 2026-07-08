# Contributing

Thanks for your interest in broom-bot! Keeping this intentionally short:

1. **Read [ARCHITECTURE.md](ARCHITECTURE.md) first** — it explains the layout, the
   command pattern, and the build system. Most questions are answered there.
2. **Build and test locally** before opening a PR. See the [README](README.md) for
   build instructions; test against your own dev guild with `DEV_GUILD_ID` set.
3. **One command per file** in `src/commands/`, registered in `all_commands.cpp`
   and `CMakeLists.txt`. Match the style of the existing commands.
4. **Add tests.** Extract a command's pure logic (parsers, match predicates) into
   a testable header and cover it in `tests/` with doctest — happy paths, edge
   cases, and invalid input. Run `broom_tests` locally; CI runs it too.
5. **Open PRs against `master`** and fill in the PR template (motivation, technical
   details, testing). CI must pass on all three platforms before merge.
6. **Never commit secrets.** `.env` is gitignored — keep it that way.

That's it. For larger changes (new core/ infrastructure, dependencies, build
changes), open an issue first to discuss.
