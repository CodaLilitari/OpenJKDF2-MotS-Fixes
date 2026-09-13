# Render and capture integration into main

Original Andrei repository PR [#4](https://github.com/andreipemberton/OpenJKDF2-MotS-Fixes/pull/4)
(historical link; that repository is now private) corrects postprocessing attribute locations and adds the background OpenGL
`-renderprobe` mode. The integration also retains the newer posed `-capture`
path, explicit multiplayer character selection, and packed-MAT row handling.
These changes were combined before native validation; the two capture modes
remain mutually exclusive.

The same merge is publicly preserved at
[`f696084bc94f9d969184ed4e282ce3dbf0915f0e`](https://github.com/CodaLilitari/OpenJKDF2-MotS-Fixes/commit/f696084bc94f9d969184ed4e282ce3dbf0915f0e), with parents
`28ec799f1dab3f3954b4d395d19711c2204c43b1` (latest capture/MAT work) and
`b2a31c7ee5cf9b9e12a32ee402af0c5ae7484c1a` (PR #4). Startup declarations and
CI branch targets were corrected in
`197c26255883fadbc7b90dcd3a2573572696058f`. The final publication adds only
documentation and workflow changes to that runtime-tested engine source.

## Integration behavior

- Postprocessing uses attribute locations from the currently linked program
  and skips absent attributes. The correction applies to ordinary gameplay.
- Render probes capture three completed composite frames as PNGs, with
  dimensions, effective settings, renderer and OpenGL-error metadata.
- Posed capture produces a HUD-free PPM and world-state JSON receipt; explicit
  multiplayer character selection remains supported.
- Both modes suppress their engine focus/input/dialog paths. Render probes
  skip the mission-briefing wait and exit on game errors instead of waiting
  for invisible dialogs. Conflicting capture requests fail before SDL startup.

See [render-probe usage](render-probe.md),
[posed capture usage](HIDDEN_RENDERED_CAPTURE.md), and
[packed-MAT handling](PACKED_MATERIAL_ROWS.md).

## Validation on 2026-09-13 UTC

The exact `197c2625` source built for Windows x64 through WSL Ubuntu with
MinGW GCC 13-posix and CMake 4.2.3. Configure and build both exited zero;
all 2,603 committed source blobs were rechecked after compilation. This build
used a copied dependency/build cache, so it is not a pristine dependency
rebuild. Existing unrelated compiler warnings remain.

The validated executable is 16,049,188 bytes, SHA256:
`4f9c4f7a10834ab76ad624e92813d89ce9a66fe4cc945e07d2782e90357c5cf3`.

Source-level capture request, launch and UI contracts passed, including an
intentionally unsafe window-raise negative control. Render-probe argument
boundaries and packed-MAT row controls passed with compiler warnings treated
as errors and AddressSanitizer/UndefinedBehaviorSanitizer enabled.

Native tests used isolated stock JK/MotS data and disposable profiles on an
NVIDIA RTX 5070 Ti, driver 616.92, hardware OpenGL 3.3:

| Check | Result |
| --- | --- |
| Stock JK Nar Shaddaa, all four bloom/SSAO combinations | 4 passed |
| Stock MotS Rebel Base, all four bloom/SSAO combinations | 4 passed |
| Posed stock MotS single-player capture | Passed |
| Stock MotS Gantry multiplayer host capture with explicit Kyle character | Passed |
| Mixed capture/probe request, headless/probe request, invalid level | All rejected with expected exit codes and diagnostics |

The eight probes produced 24 decoded, spatially varied 960x640 PNGs, with
correct requested/effective dimensions and effect flags, and zero prior and
readback OpenGL errors at the three captured frames. Engine-reported window
flags were hidden and non-focusable, without focus/grab bits, at each capture.
The two posed cases produced valid PPMs and matching world-state receipts.
The multiplayer receipt matched `kk.3do` for both requested and local models.
Stock input files and executable/DLL hashes were unchanged after testing.

All 13 intended cases passed across 14 launches. The first invalid-level
launch correctly exited 5, but the harness did not recognize the wording
"Could not load level". Its original result was retained, the assertion was
corrected, and a fresh invalid-level run passed. No engine change was needed.

These are bounded stock-map and capture-contract checks. They do not establish
complete MotS lighting parity with retail, a connected multiplayer match,
full-mod compatibility, performance benchmarks, or acceptance of an interactive
GUI session. Native captures and owned game data remain local.

## Branch preservation

`main` is the publication branch. The 13 pre-integration branch tips are
preserved by `archive/pre-main-20260913/<former-branch>` tags, including the
previous `master`, render-probe PR head and historical experimental branches.
Those tags retain comparison points without requiring a standing development
branch. Unrelated experimental histories were not merged into this integration.
