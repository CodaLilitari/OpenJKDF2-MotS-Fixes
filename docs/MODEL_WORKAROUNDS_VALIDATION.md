# Model workaround validation record

This source-only record covers the 2026-09-08 consolidation. Model artwork,
private game data, runtime staging and screenshots are not included in Git.

## Source and build identity

The renderer change was built from commit
`fe965643861c923f0bdb72361042a9d31224916a`. All 2,558 authored files matched the
exact Git archive after compilation. Later consolidation additions are the MPC
helper, synthetic tests and documentation; they do not change compiled engine
source. This was an isolated Windows x64 Release cross-build using copied
dependency caches, not a clean dependency bootstrap or a CI build.

The final version-stamped executable is 15,993,145 bytes, SHA-256
`4e8d6f3905148a90bc5e3ccb95e3108d355b935219cc7a71afc200d8de7b5e6f`.
The real source commit's full and short identifiers were verified in its
generated version definitions. Three dependency DLLs were reused from the
earlier evaluated runtime. PE imports resolve to those bundled libraries or
Windows system libraries; no separate MinGW C++/GCC/thread runtime is imported.

An earlier exact-source executable, SHA-256
`29b5a5de50feb9f12e54f58b3eae655f44a20319a08a1f7eda590d5b073d5d02`,
had an empty embedded commit string because CMake queried Git from the wrong
working directory. Its source archive and binary identity were verified; its
tests remain separately recorded. It was not relabeled as the final binary.

## Executed regression checks

- The real surfaceless Mesa OpenGL regression passed all 128 exact byte-readback
  cases and the state-restoration checks. The unprotected negative control
  reproduced the RGB row-layout failure.
- Python suites ran 47 tests: 23 KEY-domain, 14 PNG and 9 MPC tests passed; one
  native Windows symlink test was skipped for missing creation privilege.
  Deterministic linked-path/reparse refusal checks passed separately.
- The existing fork regression passed. The COG-locality regression passed
  30,766 assertions under UBSan, and its old-condition mutant was rejected.
- Independent source review found no blocking issue, and whitespace checks
  passed.

GitHub Actions dispatch returned that Actions was disabled for the account.
The workflow is configured to run the new checks, but the results above are
local executions and are not represented as a successful GitHub Actions run.

## Actual game rendering

Private Windows-engine tests under Wine64/Xvfb/Mesa llvmpipe used the original
1254-by-1254 RGB face PNG, SHA-256
`a3292b623e01a722baea8cb35385dcf19e399b53395d72a2d95e56a354df0d91`.
Its tightly packed 3,762-byte rows were not resized or converted to RGBA.
The same input previously rendered with a displaced, gray face in the affected
engine. The first newly built executable rendered the complete brown face and
features correctly in both JK and MotS; input hashes and actual PNG reads were
recorded. The JK diagnostic COG and material metadata matched the earlier
negative control; randomized multiplayer spawn backgrounds can differ.

Fresh repeats with the final version-stamped `4e8d6f39...` executable also passed
in **both JK and MotS**. Before launch, each lane verified that exact executable
hash and the original RGB image hash. All eight unchanged replacement PNGs were
read by each engine. Final inspection frames showed the correct face in both
games. Earlier setup attempts that failed the runtime-identity gate were retained
as rejected attempts and are not counted as these final passes.

These were controlled fullbright, filtered, 2x-supersampled inspection captures.
They establish this input's actual game-rendering correction in the specified
environment. Native Windows GPU behavior, remote-peer sessions, performance,
generic PBR support and original retail executable compatibility are separate
claims. See [the applicability table](MODEL_WORKAROUNDS.md) for the retail-era
animation/profile techniques and the enhancement-specific PNG path.
