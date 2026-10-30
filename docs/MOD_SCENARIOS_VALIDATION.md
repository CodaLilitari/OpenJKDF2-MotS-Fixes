# Mod scenario validation — 2026-10-30

This scripts-only increment starts from the published agent workspace commit
`8a633de0258565f9f750bcbd571f439ce08d06d9`. No native C/C++, CMake, physics,
collision, COG, game-time, weapon, save-format, or network code changed.

## Automated checks

The complete host gate passed all **15 groups** after integrating the new
scenario and report suites. Its agent group includes:

| Suite | Result |
| --- | --- |
| Native protocol/session/lease tests | All 3 groups passed with ASan and UBSan |
| Existing Python workspace suite | All 22 tests passed |
| New scenario suite | All 13 tests passed |
| New report suite | All 21 tests passed |

The 56 Python tests include a real mailbox peer process and owned child/display
fixtures. Scenario failures, wait timeouts, nonzero engine exit, cancellation
during release and display cleanup, overflowing distance calculations, relative
session paths, and bounded retry input are covered. Report tests cover exact RGB
conversion, HTML escaping, path/file boundaries, bounded inputs, and redaction.

Independent review found two controller issues: cancellation late in cleanup
could report success, and an overflowing derived distance could prevent writing
the final JSON. Both were reproduced with focused regressions before being fixed.
Report review also produced tests for alternate absolute-path forms, PPM
separator/pixel ambiguity, and bounded diagnostic-matching performance.

An unprivileged host-gate attempt encountered the already known LeakSanitizer
`/proc/.../task` visibility restriction. The final full gate ran with the needed
process visibility and passed with its sanitizer flags retained.

## Exact tested inputs

Both final real-game cases used the same native executable, scenario runner,
and launcher sources:

| Input | SHA-256 |
| --- | --- |
| Native executable | `2cb13306fd76ad394b52e89220967f865f04ee866edde7c3be1afdfc29dc323b` |
| Scenario runner | `0e4545a57b9735898c3f6021030f057975e60820996d3cd51735886bf13f5f82` |
| Workspace launcher | `112a37bbb5b895733e5396d6e6ee2fb454a7cb01c733f4d4730a13480ef9ed27` |
| Shipped JK recipe | `41ba27649c51b1ad37ba0f5b39ff37e24cc794a42d78d4c7314a87d49b98ae2d` |

The executable is the previously validated opt-in engine build. Each local
scenario receipt identifies the binary and Python source hashes separately;
the native executable was not rebuilt for Python-only changes.

## Passing real-game scenario

The shipped `examples/agent/jk-smoke.json` ran under Xvfb and software OpenGL
with local original JK data, dummy/null audio, and a 90-second execution budget.
It also staged a harmless directory through `-path startup`. That directory
contained only a text fixture; this verifies staging and launch, not resource
override correctness for a particular mod.

All **11 steps passed**. The run entered `01narshadda.jkl` through its normal
loading/objective UI, obtained a valid player, observed **0.1587562316 engine
units** of displacement after a 0.4-second Up tap, checked health and released
keys, and entered the pause menu. Four named image checkpoints were written.

The engine exited 0. The launcher recorded `completed` and `stopReason:
controller`, without forced shutdown or shutdown-error fields. The report
recorded `passed` and generated HTML, JSON, and four PNG images.

The first development attempt sent Enter during the loading screen. Its
completed-frame capture demonstrated why `worldLoaded` alone is insufficient.
The final recipe declares a bounded Enter retry until the active-player state
matches; it does not change the engine's loading behavior.

## Deliberately failing real-game scenario

A separate recipe entered the same level, held the Up key, then deliberately
required `player.health >= 999`. Observed health was **100** and the held-key
snapshot contained scancode **82**.

The assertion failed as intended. The result retained the expected/actual
comparison and last state, captured a failure image, and marked the subsequent
checkpoint `not_run`. Both the scenario and report remained `failed`; the CLI
exited 2.

The final command receipts acknowledged `release` and `quit`. The engine exited
0 through normal shutdown, while the run receipt retained the controller's
failure. Thus clean cleanup did not turn a failed test into a passing test.
The report included three selected captures.

## Evidence boundaries and limits

The local reports retain selected screenshots and bounded log excerpts, not the
original game containers, profiles, or saves. Native PPMs are retained in the
session; report PNGs preserve their RGB data. No runtime images, logs, licensed
assets, or machine-specific paths are committed to this source branch.

An independent acceptance check verified all seven selected PNGs against the
original PPM RGB bytes, resolved every capture link, and checked the report file
inventory and redaction. All original/staged GOBs, the separate startup fixture,
and generated installation markers matched their recorded hashes. Process and
display checks with runtime visibility found no remaining owned engine groups
or display endpoint; an unrelated display was left untouched.

This acceptance covers one stock JK level and a harmless startup-directory
fixture. It does not establish arbitrary-mod compatibility, resource lookup
precedence, MotS campaign behavior, deterministic replay, retail parity, or
audible playback. The supplied data did not include music; observed music-open
errors remain visible in the report without overriding the passing scenario.

The report summarizes observed diagnostics. Missing materials can fall back
silently, so a quiet log is not proof that every model, material, keyframe,
sound, or COG resource is correct.
