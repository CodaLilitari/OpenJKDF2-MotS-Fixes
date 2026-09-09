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

## Original retail follow-up

The initial Wine 10 experimental WOW64 stock-startup failure was followed by a
separate private Debian 12 i386/Wine 8/Xvfb environment. This later environment
ran original executables and copied owned resources. It did not run an OpenJKDF2
binary under a retail label. No graphics wrapper, unofficial model-limit patch,
custom KEY or custom PUP was applied to the original-JK model lanes.

| Exact executable | SHA-256 | Tested material route |
|---|---|---|
| Original JK 1.00, reconstructed to independently documented original CD hashes | `fc6d3e798da1ca940232afef217cd701d280b0f2f55907b398071e7807db548c` | Indexed MATs, original software renderer |
| Official JK 1.01, statically extracted from the publisher updater | `4509cd690634ba20e132bd16e713f91e144b17e2beaaad5e678f1ddb189b51d9` | Indexed software renderer and RGB565 builtin accelerated renderer |
| Unchanged owned MotS executable | `23cd064217f904d36d896f52d3cdd17635bb85441a12e0415c0637504191d974` | Stock gameplay control and indexed/RGB565 six-model probes after actual CD-ROM media checks |

All six preserved designs completed the original-JK probe: Specter, Revenant,
Abyssal, Organic Hybrid, Refined Hybrid and Ribbed Shroud. Each of the three
executable/material lanes requested eight representative unchanged stock KEYs
per model, for **144 successful Starweird track starts** across those lanes.
The JK 1.01 indexed lane used six independent model maps after a stock Kyle
control. The other two used the six-model sequence. Actual world captures show
the geometry and changing poses. The indexed route has a substantial brown/orange
color shift in the chosen stock map; RGB565 resolves the surface colors and
frontal rib textures more clearly.

The unchanged owned MotS executable also completed separate six-model indexed
and RGB565 probes, with **48 successful unchanged stock KEY starts in each**.
Unobscured model captures and the aggregate completion markers identify the
passing runs. Its actual accelerated display mode and stock Kyle control were
checked before the RGB565 sequence; the stock map is dark under its actual
colored lighting. An earlier indexed attempt was confounded by an optional
console helper's failure and stale frames; the affected portion was excluded.
The passing rerun used no such helper. Across the five original-engine/material
configurations, the Starweird total is **240 starts**, not 240 unique animations.

These probes entered the private episode through Single Player's Debug menu.
Their standard COG assigns each model, checks `GetThingModel`, starts an original
KEY for two seconds, and checks the returned track. Loading an MP map through
that route is not an ordinary multiplayer host or MPC-selection pass. The
accelerated lane used Wine's builtin Direct3D/Mesa software path; its real Display
menu confirmed 3D acceleration and a 16-bit mode. No native NVIDIA result is
implied.

A separate ordinary JK 1.01 host attempt created a valid stock MPC through the
original UI and selected the private episode. It then stopped at **Host Error /
Cannot connect to that network service**. Wine's builtin DirectPlay reported
`DPERR_UNSUPPORTED` from its `DP_SecureOpen` / `DPWSCB_Open` stub. No MP session
or model test began in that route. This is a recorded environment limitation,
not evidence of a Starweird or profile crash.

The JK 1.01 and owned MotS parser budgets were inspected independently:
512 vertices, 768 texture vertices and 512 faces per mesh, with at most 24
corners per face.
All six designs fit those tested budgets. However, the inspected JK 1.01 joint
builder reads the KEY node entry before checking its declared domain. Successful
track starts and survived poses do not prove absence of invalid memory reads,
every joint matrix, full stock-corpus compatibility,
sustained combat or network replication. No existing model or KEY was silently
padded to obtain these results.

The unchanged MotS stock control accepted the actual JK disc 2 marker from a
read-only mounted archived data volume and wrote its own first-run verification
state. A separately labeled compatibility ISO containing the unchanged owned
MotS marker satisfied its second CD-ROM check; this was a repack of owned files,
not an authentic MotS disc image. No synthetic marker or `InstallData` value was
written. A task-local null audio device supplied DirectSound in the private Wine
environment; Windows audio, security settings and the GPU driver were unchanged.

A separate historical four-byte no-CD reconstruction, SHA-256
`dc06b7bf7d117e772a949e725fded329eff60f76671042da64800b7a1bcadfd7`,
also passed startup, profile creation, playable stock `s1l1_rebelbase`, a
`gameover`-assisted transition through the normal results/allocation screens,
playable `s1l2_astcrust`, and normal exit with code 0. No CD drives were mapped in
that lane. The downloaded patcher was inspected as data and never executed; the
candidate was reconstructed into a separate file with exact input and byte gates.
This stock-only control is historical prior art, not a new invention or a
Starweird test. The historical function bypass leaves the return register
undefined for unmodified callers, so this bounded pass does not certify the
full campaign or every media-rebinding path.

Local receipts are retained as `jk-retail-model-results.json`,
`jk101-indexed-model-results.json`, `mots-original-model-results.json`,
`mots-rgb565-model-results.json` and
`mots-nocd-four-byte-pulse-stock/acceptance.json` in the private lab's
`validation/retail-readiness/` directory. Game executables, disc images, owned
maps, model assets and screenshots remain outside this repository.
