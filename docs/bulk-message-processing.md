# Design: Message Cache & Bulk Operations

Status: PR 1 (job system) and PR 2 (`/purge`) merged; PR 3 (cache) pending. See
ARCHITECTURE.md for the command framework this builds on.

Implemented in PR 2: `/purge channel|guild` with keywords + date ranges,
dry-run → Confirm/Cancel, bulk-delete fast path, old-message slow path,
per-channel cursor resume. Known v1 limitations: scanning is sequential (no
cross-channel parallelism yet); archived threads are not enumerated; the
`source` option is deferred to PR 3 (no cache exists yet).

## Goals

Bulk operations over guild message history (e.g. delete every message matching
keywords), scoped per-channel or guild-wide, with date ranges. Long-running by
nature - jobs must survive restarts and respect Discord rate limits.

## Non-negotiable API constraints

- Slash commands must ack in ~3s → all heavy work runs as background jobs with
  progress reporting (edited status message, throttled) and a completion message.
- Reading history: REST pagination, 100 msgs/request, parallelizable across
  channels under the global (~50 req/s) ceiling. ~1M messages ≈ 30–90 min scan.
- Deleting: `bulk_delete` (100/call) works only on messages **< 14 days old**;
  older messages delete one-by-one at ~1–5/s per channel. Deleting 10k old
  messages ≈ hours. **No caching can accelerate deletion** - only discovery.
- DPP handles 429s/buckets automatically; we add bounded channel concurrency and
  stay below the global ceiling so interactive commands never starve.
- Live message events require the privileged Message Content intent; historical
  REST scans do not.

## Privacy model: caching is opt-in, explicit, and clearable

No automatic indexing on guild join. Admins run an explicit command:

- `/cache build [channel] [from] [to]` - index message metadata+content into
  SQLite. `channel` omitted = entire guild. `from`/`to`: ISO dates
  (YYYY-MM-DD); defaults: beginning of history → now.
- `/cache update [channel]` - forward-fill each covered channel from its newest
  cached message to now (manual catch-up; no new ranges).
- `/cache status` - coverage (per-channel indexed ranges, row counts, freshness).
- `/cache clear [channel]` - drop cached data (whole guild if no channel).

## Settings: freshness policy is per-guild, stored in the same DB

`guild_settings` table (SQLite - the DB is the per-guild settings store; `.env`
is process config only). Cache **coverage** (what was indexed) and **freshness
policy** (how it stays current) are deliberately decoupled - a guild can have a
full cache that only updates on demand.

`/cache settings ingest:<manual|live>`:

- **`manual` (default)** - cache changes only via `/cache build` / `/cache
  update`. Pure REST: the bot needs **no privileged intent** in this mode.
- **`live`** - gateway ingest (`message_create/delete/update`) for covered
  channels + automatic gap backfill on startup. Requires the privileged Message
  Content intent (Developer Portal toggle; Discord approval needed at 75+
  guilds - another reason manual is the default).

## Offline gaps (live mode) / staleness (manual mode)

- Messages **sent** while offline (live mode): recovered on startup by forward
  pagination (`after=<newest cached id>`) per covered channel. In manual mode
  the same mechanism runs only via `/cache update`.
- Messages **deleted** while offline: self-healed lazily - a delete-by-id that
  404s removes the stale row.
- Messages **edited** while the bot was offline (or any time, in manual mode):
  **known limitation, accepted.** The cache may hold pre-edit content until the
  range is re-built (`/cache build`).

## Bulk commands

- Scope is expressed as a **subcommand**, not an argument - guild-wide deletion
  is a distinct command path you cannot reach by omitting an option:
  - `/purge channel keywords:<w> [target] [from] [to] [source]` - `target`
    defaults to the invoking channel.
  - `/purge guild keywords:<w> [from] [to] [source]`
  Date defaults: all history. (Cache commands keep a single optional `channel`
  argument where omitted = entire guild: building a cache is reversible, so
  defaulting wide is safe there; deleting is not, so purge makes you say it.)
- Flow: **always dry-run first** - job counts matches, replies "N messages,
  ~T estimated - Confirm / Cancel" buttons; deletion only on confirm.
- Bulk commands take a `source` option controlling discovery only (deletion
  speed is identical regardless - rate limits bind either way):
  - `auto` (default): use cache where coverage exists, live-scan uncovered
    ranges. Dry-run report states the source and cache age.
  - `scan`: ignore the cache, always walk history fresh via REST.
  Purge never requires a cache to exist.
- Permission gating: `default_member_permissions` = Manage Messages (purge) /
  Manage Guild (cache); bot's own permissions checked per channel and reported.

## Job system (core/jobs)

- SQLite-backed, resumable: jobs + per-channel cursors (pagination `before`/
  `after` ids are natural checkpoints) + counters. Restart → resume from cursor.
- One active job per guild; queue if busy. Cancel button on the progress message.
- Progress edits throttled (~1 per 3s max).
- Deletion idempotent: 404 → count as already-gone, continue.
- Threads and forum posts require separate enumeration from text channels -
  in scope for scanning, tracked per-container like channels.

## Schema sketch

```
guild_settings(guild_id PK, ingest_mode, ...)       -- per-guild policy
coverage(channel_id, guild_id, from_id, to_id)      -- indexed ranges (fact)
messages(id, channel_id, guild_id, author_id, created_at, content)
jobs(id, guild_id, kind, params_json, status, created_at, finished_at)
job_cursors(job_id, channel_id, cursor_id, scanned, matched, deleted)
```

## Implementation sequence (one PR each)

Note: the headline use case (one-time keyword purge over full history) requires
no cache - purge is scan-and-delete either way. The cache only accelerates
*repeated* sweeps, so it lands last and can be re-scoped once purge is in use.
SQLite is justified by PR 1 alone (resumable cursors must survive restarts).

1. **core/jobs + SQLite** - job runner, persistence, progress/cancel plumbing.
2. **/purge (channel + guild scope, live scan)** - end-to-end vertical slice:
   dry-run, confirm, bulk-delete fast path, old-message slow path, progress,
   resume.
3. **/cache + cache-accelerated purge** - build/update/status/clear/settings,
   manual vs live ingest modes, gap backfill.

## Dependency note

SQLite (public domain) will be vendored or fetched via CMake - must not break
the three-platform build-from-source story. Redis was considered and rejected:
wrong durability model for cursors, extra server dependency, no benefit at
single-process scale.
