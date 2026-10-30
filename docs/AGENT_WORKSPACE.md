# Linux agent workspace

This branch adds an **opt-in local controller for live single-player sessions**.
It supports ordinary keys and mouse movement, menu input, read-only game state,
current-view screenshots, and orderly shutdown. A Python tool stages disposable
sessions and can run them on a private Xvfb display with software OpenGL.

The game continues to use its normal clock, physics, collisions, COGs, input
bindings and save/load code. No command teleports a player, changes health,
sets velocity, advances the simulation by a synthetic timestep, or runs console
code. Input commands enter the normal input handling path. This is a test and
play interface; it does not establish deterministic replay or retail parity.

## Build

Use the fork's usual native Linux prerequisites and pinned submodules. Add the
feature flag to a separate build directory:

```sh
git submodule update --init --recursive
cmake -S . -B build_agent_linux \
  -DPLAT_LINUX_64=ON -DCMAKE_BUILD_TYPE=Release \
  -DOPENJKDF2_AGENT_HOOKS=ON
cmake --build build_agent_linux --target PROTOBUF --parallel 4
cmake --build build_agent_linux --target openjkdf2 --parallel 4
python3 -B scripts/agent_workspace.py doctor \
  --binary build_agent_linux/openjkdf2
```

The `PROTOBUF` preparation follows the existing native build workflow when
GameNetworkingSockets is built from the bundled sources. If CMake found an
installed GameNetworkingSockets package and did not create that target, build
`openjkdf2` directly. The source directory still uses the legacy `SDL2` name;
this fork's native build uses its pinned SDL3/SDL3_mixer dependencies.

`OPENJKDF2_AGENT_HOOKS` defaults **OFF**. Enabling it requires the native Linux
SDL/OpenGL backend. Even an enabled binary activates the adapter only with
`-agent`, a private command directory and an isolated working-directory marker.
`-agent` on a build without the feature fails explicitly.

The Python tool needs Python 3.11 or newer and only standard-library modules.
For a display-less workspace, install Xvfb and Mesa's software GL driver using
the operating system's package manager. The tool does not install dependencies
or download game data.

## Prepare a session

Supply a local installed game tree containing `resource/` and `episode/`:

```sh
python3 -B scripts/agent_workspace.py prepare \
  --data-root /path/to/your/JK \
  --session /tmp/openjk-session-001
```

The destination must be new and outside the source installation. Asset trees
are copied, so writes inside the session cannot change the originals. Existing
profiles and settings are not imported. The manifest records each copied
file's size and SHA-256. The normal engine install marker is created locally
when absent; an existing marker must match the selected game.

Use `--game mots` for MotS data. The tool validates that the expected resource
and main episode containers exist; it does not certify their contents. Asset
symlinks are rejected, so use a real asset directory rather than a previously
symlinked test stage.

For a startup mod and additional level container:

```sh
python3 -B scripts/agent_workspace.py prepare \
  --data-root /path/to/your/JK \
  --startup /path/to/mod-directory \
  --episode-file /path/to/custom-episode.gob \
  --session /tmp/openjk-mod-001
```

The startup mod is copied into the session's `startup/` and launched through
the existing `-path startup` behavior. Additional episodes are copied into
`episode/`. Conflicting destinations are rejected rather than overwritten.

## Launch and control

Launch a bounded session in one terminal:

```sh
python3 -B scripts/agent_workspace.py run \
  --binary build_agent_linux/openjkdf2 \
  --session /tmp/openjk-session-001 \
  --episode jk1 --map 01narshadda.jkl \
  --xvfb --software --seconds 180
```

The launcher prints a JSON `ready` event once the command endpoint answers.
This acknowledges the controller, not necessarily completion of a level load;
query `state` and check `levelActive` and `player` before gameplay assertions.
The stock first level initially shows its objective screen. Capture the view
and press Enter to dismiss that screen through the ordinary menu, then wait
for `levelActive: true` and a non-null player.
If no `DISPLAY` exists, the launcher creates its own Xvfb display automatically.
`--software` requests Mesa software rendering. Audio defaults to dummy/null
backends; use `--audio` when an actual audio device is wanted. Omitting `--xvfb`
uses an existing display when available, allowing a person to watch and play.

In another terminal, or another tool call:

```sh
python3 -B scripts/agent_workspace.py command --session /tmp/openjk-session-001 state
python3 -B scripts/agent_workspace.py tap --session /tmp/openjk-session-001 Enter
python3 -B scripts/agent_workspace.py tap --session /tmp/openjk-session-001 --seconds 0.4 Up
python3 -B scripts/agent_workspace.py command --session /tmp/openjk-session-001 mouse 35 -10
python3 -B scripts/agent_workspace.py command --session /tmp/openjk-session-001 capture
python3 -B scripts/agent_workspace.py tap --session /tmp/openjk-session-001 Escape
python3 -B scripts/agent_workspace.py command --session /tmp/openjk-session-001 release
python3 -B scripts/agent_workspace.py command --session /tmp/openjk-session-001 quit
```

Named keys include letters, Digit0–Digit9, arrows, Escape, Enter, Tab, Space,
PageUp/PageDown, modifiers and F1–F12. The `key` wire command uses numeric SDL
scancodes, not Windows virtual-key codes or JK DirectInput codes. Bare numbers
passed to `tap` are also scancodes; use `Digit4` for the number-row 4 key. The engine's
normal profile bindings determine what those keys do. F9 is the stock JK
quicksave binding; ordinary menu navigation performs load operations.

`mouse DX DY` supplies relative motion and moves a virtual pointer for menus.
`state` supplies pointer coordinates plus window and drawable dimensions to
relate menu clicks to screenshots. `button N DOWN` uses SDL button numbers:
1 left, 2 middle, 3 right, 4 and 5 side buttons. `text YOUR TEXT` types up to
127 printable ASCII characters into the ordinary text-input path.

Held keys/buttons automatically release after **2000 ms without another input
command**. State queries and screenshots do not refresh that lease. Use `tap`
for short presses, periodic input commands for an intentional longer hold, and
`release` to clear all injected input. Real physical input is still available.

## Read state and inspect the real view

`state` returns the current GUI/menu/level flags, monotonic and game time,
held inputs, window sizes, and a player snapshot when the current world is
valid. The snapshot includes position, velocity, health, sector, thing ID and
head orientation. During loads or states without a valid player, `player` is
`null`. Test the availability flags rather than assuming every query is in play.

`capture` writes `agent/frame-ID.ppm` only after a rendered frame is read and
the file is complete. Its response names the relative file, dimensions, frame
counter and monotonic timestamp. Files are new and never silently overwritten.
The image is a standard top-down RGB PPM and may be opened directly or converted
with a local image utility. The pixels represent the displayed game/menu view;
the hook does not replace the camera, move the player or hide the HUD.
During a menu transition or level load, a completed frame can still be a blank
or intermediate view. Wait for the intended state and inspect a later capture
before making assertions about the destination screen.

The existing `-capture`/`sed-engine-capture-v1` posed inspection workflow is
separate and unchanged. It cannot be combined with `-agent`.

## Scripted tests

For a reusable JSON recipe with assertions, named checkpoints, failure evidence,
and an HTML/PNG report, see [MOD_SCENARIOS.md](MOD_SCENARIOS.md). The included
stock JK recipe exercises the first level through ordinary controls. Adapt its
launch target and assertions for a candidate mod.

The same client is importable without packaging or an additional dependency:

```python
import sys
sys.path.insert(0, "scripts")
from agent_workspace import Client

client = Client("/tmp/openjk-session-001", timeout=10)
before = client.request("state")["result"]
assert before["levelActive"] and before["player"] is not None
client.tap("Up", seconds=0.4)
after = client.request("state")["result"]
image = client.request("capture")["result"]
client.request("release")
print(before["player"]["position"], after["player"]["position"], image)
```

Movement is affected by real walls, enemies, physics and bindings; a successful
input receipt is not proof that a desired gameplay outcome happened. Write
scenario assertions against the resulting state and pixels. Wall-clock waits
are ordinary real-time play, not frame-exact simulation stepping.

## Receipts and failure behavior

The private `agent/` directory contains `command.txt`, `response.json`, command
sequence metadata, and `commands.jsonl`. Commands are serialized using a local
file lock; requests carry increasing IDs, and the client accepts only a reply
matching the current ID and command. A timeout preserves an unconsumed request
instead of replacing it. Engine rejection is a nonzero CLI result. Each request
and receipt is retained in the command log.
An engine-side lock also prevents two running engines from consuming the same
session's mailbox.

The session also contains `session.json`, `run.json`, `engine.stdout.log`,
`engine.stderr.log`, `debug.log` when produced by the engine, and `xvfb.log` when
the launcher owns a display. Profiles, save files and XDG paths stay local to
the session. Run receipts record the executable hash, exact launch arguments,
completion reason and exit status. A session is launched only once; prepare a
new one for another independent run.

At the run limit the launcher requests release and quit, then terminates only
its own process group if the engine does not finish. Display cleanup runs on
success, error and interruption. No TCP control listener is created.

Agent mode is restricted to local single-player use. It rejects dedicated,
multiplayer, headless, posed-capture, render-probe and DroidWorks launch modes,
and refuses command control if the game later enters multiplayer. Normal
multiplayer behavior remains outside this interface.

## Tests and scope

```sh
python3 -B src/Tests/test_agent_workspace.py
python3 -B src/Tests/run_agent_tests.py
python3 -B src/Tests/run_host_regressions.py run --output /tmp/openjk-host-new
```

The host tests use synthetic fixtures and require no licensed data. Runtime
acceptance needs the user's game installation. See
[AGENT_WORKSPACE_VALIDATION.md](AGENT_WORKSPACE_VALIDATION.md) for the exact
checks actually run on this branch and their limits.
