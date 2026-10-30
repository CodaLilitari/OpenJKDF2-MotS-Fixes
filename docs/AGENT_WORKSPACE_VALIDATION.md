# Agent workspace validation — 2026-10-30

## Source and scope

The branch starts from `CodaLilitari/OpenJKDF2-MotS-Fixes/main` at
`ea715f21d8bd54ec2f2c5cc55ad710d851b8e442`. The unmodified baseline and the
candidate were built separately on Ubuntu 24.04 x86_64, using GCC 13.3 and
CMake 3.28.3. Runtime tests used Xvfb, Mesa software OpenGL, dummy/null audio,
and locally supplied original Jedi Knight single-player data.

Only the candidate enables `OPENJKDF2_AGENT_HOOKS`. The option defaults OFF,
and an enabled binary requires explicit `-agent` activation and a private
session. The change adds input/observation and launch supervision; the diff
does not change physics, collision, COG execution, game-time calculation,
weapon behavior, save serialization, or network protocols.

This is evidence for the checks listed below. It is not a claim of retail
parity, deterministic simulation, or coverage of every level and game mode.
Licensed game content, screenshots, saves, binaries, and machine-specific logs
are not included in this source branch.

## Build identity

Both native Release builds succeeded and their runtime shared libraries
resolved. All candidate game objects were compiled from the candidate source.
Bundled dependency build products were reused only after comparison of 30
normalized configuration records and 33 binary hashes. No baseline game
objects were copied into the candidate build.

| Item | SHA-256 |
| --- | --- |
| Unmodified baseline executable | `5120832f600b195dad8e84bfb8dfeee8d8303e63f37531a72d90aac93cd592d5` |
| Tested candidate executable | `2cb13306fd76ad394b52e89220967f865f04ee866edde7c3be1afdfc29dc323b` |
| Candidate native-input manifest | `146f705c8271be60d6e5ae79ed60fa7eb727d44056c36b7dfcba884d258bd485` |

The candidate was built before committing the source changes. Its embedded
Git version therefore identifies the base commit; the executable and the
1,242-entry native-input manifest above identify the tested build. The inputs
were unchanged across its final build. Documentation updates do not affect
those native inputs.

## Automated checks

| Check | Observed result | Scope |
| --- | --- | --- |
| Unmodified host regression gate | All 14 groups passed | Existing source-only tests, with their configured sanitizers retained |
| Candidate host regression gate | All 15 groups passed | Existing groups plus the new agent-workspace group |
| Native agent tests | All 3 groups passed with ASan and UBSan | Actual protocol parser, input lease, private session/mailbox, receiver lock, output files, and failure cases |
| Python workspace tests | All 22 tests passed | Asset isolation, protocol peer exchange, strict receipts, timeouts, writer locking, command history, and child/display lifecycle |
| Host-runner self-tests | All 5 tests passed | Failure/timeout reporting and artifact allowlist |
| Missing private session | Enabled binary exited 6 | Explicit rejection before game startup |

The native agent unit runner links `stdAgentProtocol` and `stdAgentSession`.
It does not link SDL or run the main adapter; the live checks below cover that
integration. The Python suite uses synthetic assets and a protocol peer and
does not establish gameplay correctness by itself.

LeakSanitizer could not inspect processes during the first restricted-sandbox
attempt. The unchanged baseline gate was rerun with the required process
visibility and passed. Sanitizers were not disabled to obtain a passing run.

Reproduce the automated checks from the repository root:

```sh
python3 -B src/Tests/test_host_regression_runner.py
python3 -B src/Tests/run_host_regressions.py run --output /tmp/openjk-host-new
```

The second command includes the three native groups and the 22 Python tests.
Its receipt states explicitly that no game was launched.

## Hooks compiled in, without activation

The existing `-capture` workflow was run separately with the unmodified
baseline and the enabled candidate, omitting `-agent` in both cases. Each
captured `01narshadda.jkl`, sector 339, position
`[-10.4434328, -5.45509386, -1.12988603]`, orientation `[0, 90, 0]`, horizontal
FOV 90, at rendered frame 3 and 640×480 resolution.

Both processes exited 0. The images were byte-identical, with SHA-256
`c5f935a570596351e0d3b349da336aa60c6c8dc2eefa00c40a1ea30551ad4df5`, and the
engine sidecars were identical. Staged source-asset hashes remained unchanged.
All display and engine processes owned by the two checks exited.

For the candidate run, `OPENJKDF2_AGENT_DIR` named a valid private directory
containing a syntactically valid `quit` command. That mailbox remained
unchanged; no agent response or engine lock was created. This demonstrates
inactivity in this particular launch, not exhaustive inactive-mode parity.

## Live play

The live scenario uses the stock first level, `jk1` / `01narshadda.jkl`, through
the ordinary objective screen, controls, pause menu, and save/load menus.
The following results were observed with the tested candidate executable:

| Action | Observed result |
| --- | --- |
| Dismiss objectives with Enter | Entered the level with a valid world and player snapshot |
| Hold the ordinary Up key for 0.4 seconds | Player position changed by approximately 0.0737 world units |
| Send `mouse 35 -10` | Head pitch changed from 0 to 3 and the view changed |
| Press and release mouse button 1 | The weapon fired; visible ammunition changed from 50 to 49 |
| Hold Left while sending only state queries | Key remained held at 1.69 seconds and was released at 2.02 seconds |
| Use the stock F9 quicksave binding | Created a 360,401-byte quicksave in the session profile |
| Move after saving, then load through the pause/load menus | Restored the saved position exactly, with a valid player in the same world |
| Capture objectives, play, pause, load, and confirmation screens | Produced completed 640×480 current-view images with ordinary HUD/menu content |
| Request quit with the nested Abort Game confirmation open | Command 68 acknowledged release; engine and launcher exited 0, with a completed run receipt |

For the save/load check, the saved position was
`[-10.369894, -5.46035671, -1.12987673]`. Before loading, ordinary movement had
changed it to `[-10.4519072, -5.47435236, -1.12989581]`. Loading restored the
saved position exactly. The observed game time changed from 45,835 to 29,384 ms.
The hooks did not write either value; the normal save/load implementation
performed the restoration.

An earlier integration executable exposed the engine's historical exit code
1 after an acknowledged quit. The candidate explicitly returns 0 after agent
quit completes ordinary engine and filesystem cleanup. Non-agent exit behavior
is unchanged. The final quit completed before the run limit, and the engine log
reached `Main_Shutdown done`; the launcher reported `stopReason: engine-exit`.
The known launcher, Xvfb, and engine processes and their process groups were
absent after cleanup. SHA-256 checks confirmed that all three original GOBs
and their staged copies were unchanged.

## Limits

MotS-specific session staging and command-line support are implemented and
covered with synthetic file tests. A MotS campaign was not launched because
its game data was unavailable in this workspace. Multiplayer, DroidWorks,
mobile platforms, GL 1.1, hardware GPU rendering, and audible playback were
not acceptance-tested for this interface. Agent mode deliberately excludes
multiplayer and incompatible inspection modes.
The supplied game stage did not include music files; the normal engine logged
missing-music probes while null audio was selected. Printable text input was
covered by the protocol/client tests but was not exercised in a live save dialog.

Startup-mod staging is tested for copy isolation and argument validation.
Support for a staged `-path startup` does not imply that arbitrary mods have
been validated. The ordinary engine decides resource precedence and mod
behavior.

Inputs run on the normal real-time clock. The observed result of a short key
press depends on scene content, bindings, load, and frame cadence. Captures
include the real HUD/menus and may catch an intermediate frame during a
transition; callers must wait for the state they intend to inspect. There is
no frame stepping, teleporting, invulnerability, or simulation override.
