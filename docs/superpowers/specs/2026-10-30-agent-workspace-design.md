# Linux agent workspace

## Intent and constraints

Build a separate GitHub fork branch that makes OpenJKDF2 easier to launch,
play, observe and test in a Linux automation workspace. The user authorized
agent hooks and workspace adaptations while explicitly preserving physics and
fundamental gameplay. Base: `ea715f21d8bd54ec2f2c5cc55ad710d851b8e442` on
`CodaLilitari/OpenJKDF2-MotS-Fixes/main`.

Physics, collision, COG execution, game time, weapon behavior, save formats and
network protocols are outside the implementation scope. Tests use the ordinary
simulation clock. No deterministic replay or retail-parity claim is implied.
Original game data remains local and is excluded from Git and CI artifacts.

## Approaches and implementation decision

External desktop input alone would reuse native event polling, but it would
depend on focus and would expose neither reliable state nor completed-frame
receipts. Existing `-capture` provides useful posed inspection, but moves the
camera and suppresses controls. A separately enabled local input/observation
adapter addresses live play with a small, reviewable integration surface.

The implementer's selected approach is a Linux-only build option,
`OPENJKDF2_AGENT_HOOKS`, OFF by default, plus explicit `-agent` activation and
an isolated session. This is a choice under the user's implementation mandate,
not a separately approved design. Normal builds and normal launches keep their
existing behavior. The current inspection capture API remains intact.

## Native interface

`OPENJKDF2_AGENT_DIR` points to a private directory owned by the current user.
The staged working directory contains `.openjkdf2-agent-session` and valid local
game resources. Agent mode rejects incompatible capture/headless/render-probe,
dedicated/multiplayer and DroidWorks launch modes. Commands cannot operate on a
later multiplayer session.

The single mailbox `command.txt` is atomically installed. Its UTF-8/ASCII wire
format is `openjkdf2-agent-v1\n<ID>\n<command>\n`, at most 4096 bytes. IDs are
strictly increasing integers from 1 through 2147483647. One command is in flight
at a time. `response.json` is atomically replaced and carries `schemaVersion:1`,
`id`, `ok`, `command`, and either `result` or `error`.

Commands: `ping`, `state`, `key SCANCODE DOWN`, `mouse DX DY`,
`button BUTTON DOWN`, `release`, `capture`, `quit`, and printable ASCII `text`.
Scancodes are 1..255, buttons 1..5, down is 0 or 1, mouse deltas are bounded by
2048. Held input expires after 2000 ms without another input command; observation
does not renew it. This is an input safety lease, not a simulation time change.

Ordinary SDL event routing handles menus and mouse movement. A held-key overlay
also participates in the existing keyboard polling and debounce path. State is
read only and validates current world/player lifetime before dereferencing it.
Capture reads the completed current view before presentation and writes a new
`frame-ID.ppm` in the private directory; it never applies a camera pose. It
returns the path and dimensions only after successful output, or a bounded
failure. Quit uses the engine shutdown path, including nested menus.

## Workspace tooling

A Python standard-library launcher/client prepares a new private session from
an existing local game installation, copies only explicit asset directories,
and keeps profiles, saves, logs and receipts inside the session. It supports
staged startup mods and additional episode containers. It never installs game
data or downloads it automatically.

The launcher can create its own Xvfb display, select software GL and null audio,
and impose a wall-clock run limit. It supervises and terminates only its own
children. The client serializes writers with a lock, validates arguments,
matches receipts by ID, records command history, and fails on timeouts or engine
errors. CLI commands and an importable client support play/test scripts.

## Acceptance

The asset-free baseline gate must remain green. New native tests cover the
actual parser, invalid/oversized commands, input expiry, and inactive behavior.
Client tests cover isolated staging, argument validation, stale receipts,
timeouts, one-writer locking, and cleanup. Build both unmodified baseline and
enabled branch. With available licensed assets, launch under Xvfb, observe
state, capture a real rendered view, move using normal controls, release input,
exercise menus/save/load as practical, and quit. Record observed limits without
turning source-level checks into claims of full gameplay parity.
