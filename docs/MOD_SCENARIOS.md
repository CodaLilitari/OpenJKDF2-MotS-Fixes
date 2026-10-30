# Repeatable mod scenarios and reports

The scenario tool turns the agent interface into a bounded mod smoke test:
stage a candidate, wait for a known game state, exercise normal controls, check
the outcome, and retain named screenshots and state. It uses the existing
launcher and native adapter. This increment adds no native engine changes.

## Run the included scenario

Build the opt-in executable as described in [AGENT_WORKSPACE.md](AGENT_WORKSPACE.md),
then run this from the repository root:

```sh
python3 -B scripts/agent_scenario.py \
  --scenario examples/agent/jk-smoke.json \
  --binary build_agent_linux/openjkdf2 \
  --data-root /path/to/your/JK \
  --session /tmp/jk-smoke-001 \
  --xvfb --software --seconds 90
```

The tool creates a new isolated session and a report at
`/tmp/jk-smoke-001/report/index.html`. Keep the report folder together: its
`captures/` directory contains the PNGs referenced by the HTML. `summary.json`
provides the same evidence in machine-readable form. The raw session retains
the complete command journal, original PPMs, engine logs, and run receipts.

The included recipe targets the **stock JK first level**, `01narshadda.jkl`.
It waits for loading, dismisses the objective screen, checks that a valid player
is active in that world, measures ordinary forward movement, and opens the pause
menu. Four checkpoints capture startup, spawn, movement, and the pause screen.
It does not certify an arbitrary replacement level or mod.

The process exits 0 only if every scenario step passes, the engine exits cleanly,
and the requested report is generated. It exits 2 on failure. A failed assertion
remains a failed test even when subsequent shutdown succeeds.

## Test a candidate mod

Copy the JSON recipe, set its `launch` fields and observations to the intended
level, and pass the candidate's startup directory or extra episode container:

```sh
python3 -B scripts/agent_scenario.py \
  --scenario /path/to/my-mod-check.json \
  --binary build_agent_linux/openjkdf2 \
  --data-root /path/to/your/JK \
  --startup /path/to/candidate-startup \
  --episode-file /path/to/candidate-episode.gob \
  --session /tmp/my-mod-check-001 \
  --xvfb --software --seconds 180
```

The existing staging tool copies those inputs and records file hashes. Saves
and profiles remain inside the new session. To test another version, use another
new session with the same recipe. Compare the named captures and observed state;
real-time input does not imply identical outcomes on every run.

The recipe selects JK or MotS with `launch.game`. MotS staging is supported,
but this increment's live acceptance used JK data. `--audio` selects available
audio devices; otherwise the launcher uses dummy/null audio. `--no-report`
retains the JSON, logs, and PPMs without creating HTML or PNG copies.

## Recipe format

Recipes are JSON data, with no executable expressions or external commands.
Unknown fields and unsupported actions are rejected before a session is created.

```json
{
  "schemaVersion": 1,
  "name": "Candidate starting area",
  "launch": {
    "game": "jk",
    "episode": "jk1",
    "map": "01narshadda.jkl"
  },
  "steps": [
    {
      "op": "wait",
      "name": "ready",
      "expect": {
        "world": "01narshadda.jkl",
        "levelActive": true,
        "menuActive": false,
        "player": {"notNull": true}
      },
      "timeout": 30,
      "retryTap": {"key": "Enter", "seconds": 0.1, "every": 1}
    },
    {"op": "checkpoint", "name": "before"},
    {"op": "tap", "key": "Up", "seconds": 0.4},
    {
      "op": "assert",
      "name": "movement",
      "expect": {"player.health": {"min": 1}, "heldKeys": []},
      "distanceFrom": {"checkpoint": "before", "min": 0.005, "max": 3}
    },
    {"op": "checkpoint", "name": "after"}
  ]
}
```

| Operation | Fields | Behavior |
| --- | --- | --- |
| `wait` | `expect`, optional `timeout`, `stableFor`, `retryTap` | Poll state until all comparisons match for the requested duration |
| `assert` | `expect` and/or `distanceFrom` | Read state once and fail if a comparison does not hold |
| `checkpoint` | Optional `capture` boolean | Save state under the step's name; capture the current view by default |
| `tap` | `key`, optional `seconds` | Press and release an ordinary named key or SDL scancode |
| `command` | `value` | Send a validated `key`, `mouse`, `button`, `text`, or `release` command |
| `sleep` | `seconds` | Wait ordinary wall-clock time while retaining cancellation/deadline checks |

Every step may have a unique `name` containing letters, digits, underscores,
periods, or hyphens. If omitted, the tool assigns one. Give checkpoints explicit
names when later assertions reference them.
Checkpoint state is read before its capture request. The game continues between
commands; timestamps and frame counters distinguish these successive observations.

### State comparisons

An `expect` object maps dot-separated fields to an exact JSON value or a numeric
range. Examples:

```json
{
  "world": "01narshadda.jkl",
  "player": {"notNull": true},
  "player.health": {"min": 1, "max": 100},
  "player.sector": 339,
  "player.position.2": {"min": -1.3, "max": -1.0},
  "paused": false,
  "heldButtons": []
}
```

Array indices use the same dotted notation. Missing fields differ from explicit
`null`; boolean `true` differs from numeric 1. Numeric comparisons require
finite values. A `distanceFrom` assertion measures Euclidean displacement in
engine units from an earlier checkpoint's player position. Use a small `max`
tolerance to check restoration after an ordinary save/load sequence. The tool
never restores the checkpoint itself.

The current state interface does not expose ammunition, inventory, COG variables,
menu text, active enemy counts, or the origin of a selected resource. Check
screenshots where those observations matter. `headPYR` is head orientation;
`thingCapacity` is array capacity.

### Loading and bounded retries

The endpoint's ready ping and `worldLoaded: true` can both occur during the loading
screen. They do not establish that a player is in active gameplay. The stock
objective screen needs an ordinary Enter press after loading finishes.

`retryTap` explicitly requests a short key press whenever a wait still does not
match and its retry interval has elapsed. It stops sending input as soon as all
comparisons match; `stableFor` can then require the state to remain matched.
The stock recipe uses this to avoid losing its only Enter press during loading.
Use retries only for a key whose repeated effect is appropriate to that specific
menu/level. The runner does not add retries to recipes that omit them.

Wait timeouts are at most 120 seconds; sleep steps at most 30 seconds; taps at
most 1.5 seconds. The overall execution budget is 1..3600 seconds after the
endpoint is ready, with the launcher's separate bounded startup and shutdown
periods. Recipes allow at most 256 steps and 31 explicit image checkpoints.
One additional image slot is reserved for failure evidence.

State polling and captures do not renew the native two-second input lease.
`command` can deliberately hold an input, but the recipe must keep sending
ordinary input if it intends to maintain that hold.

## Failure evidence and cleanup

`scenario.json` is updated as each step runs. On a failed assertion, the record
retains expected versus actual comparisons, the last state, command IDs, and a
failure screenshot when the engine and remaining time budget allow it. Later
steps remain explicitly `not_run`.

The launcher attempts `release` and `quit` after success or failure, then cleans
up only its owned processes. A controller error, engine crash, forced stop,
unacknowledged shutdown, or cancellation prevents a successful result. The
original assertion error survives additional cleanup failures when possible.

The report includes hashes for the recipe, runner, launcher, binary, and staged
asset manifest, along with relative asset names and their sizes/hashes. The
tested executable can be identified even when its embedded Git version predates
the current Python tooling.

## Reports from existing sessions

For a stopped manual session, or to rebuild a report into another new directory:

```sh
python3 -B scripts/agent_report.py \
  --session /absolute/path/to/session \
  --output /absolute/path/to/new-report
```

Use canonical paths without `..` or symlinked components for this standalone
command. The scenario runner resolves its prepared session automatically.
Without a scenario record, the report selects up to 32 existing captures in
numeric command-ID order.

The report scans bounded portions of `engine.stdout.log`, `engine.stderr.log`,
and `debug.log`. Matches carry source filenames and line numbers. Model fallback,
archive, material, keyframe, COG parser, and audio messages are observations;
they do not determine the scenario's pass/fail status. Some missing materials
fall back silently, and missing optional music can emit errors during a
successful game session.

Report metadata reads are capped at 2 MiB per file and 4,096 asset entries;
each log scan reads at most 256 KiB
and retains at most 64 matching lines. Truncation is stated in the report. PNGs
preserve the original RGB pixels, subject to native image dimensions and a
64-megapixel aggregate budget. The report contains selected screenshots and
excerpts, but no GOB/GOO assets, saves, profiles, full launch command, environment,
or absolute source paths. It is created locally and is not uploaded automatically.

## Validation

The source-only host gate includes all three Python suites:

```sh
python3 -B src/Tests/test_agent_workspace.py
python3 -B src/Tests/test_agent_scenario.py
python3 -B src/Tests/test_agent_report.py
```

The scenario tests use a real mailbox peer process, including successful movement
checks, assertion failure, timeout, nonzero exit, cancellation during shutdown
and display cleanup, overflowing distance calculations, relative paths, and
bounded retry input. The report tests validate exact pixel conversion, HTML
escaping, bounded input, and file/privacy boundaries.

See [MOD_SCENARIOS_VALIDATION.md](MOD_SCENARIOS_VALIDATION.md) for the real-game
acceptance results and their limits.
