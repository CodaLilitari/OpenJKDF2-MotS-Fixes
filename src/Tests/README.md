# Fork host regression gate

Run the same asset-free gate locally and in CI from the repository root:

```sh
python3 -B src/Tests/run_host_regressions.py run --output /tmp/openjk-host-results
```

The output directory must not already exist. A native Linux host needs Python 3,
GCC/G++, CMake, Git, Pillow, the EGL/OpenGL development libraries, and Mesa's
surfaceless software renderer. On Ubuntu these system dependencies are provided
by `build-essential cmake git python3 python3-venv libegl-dev libgl-dev
libgl1-mesa-dri`; install `Pillow==12.3.0` in a virtual environment. Engine
submodules and licensed game data are not required. `--cc` and `--cxx` select
native compilers; do not select a cross compiler.

The gate preserves the existing tests and their sanitizer settings:

| Group | Sanitizers / scope |
| --- | --- |
| Fork helpers | Inventory bindings and material-cache helpers |
| COG locality | UBSan |
| Backpacks | UBSan |
| Team models | UBSan |
| Bitmap conversion | UBSan |
| MotS lighting and placed-thing loader | ASan + UBSan |
| POV lighting | ASan + UBSan |
| Item take behavior | ASan + UBSan |
| Capture contract and UI stubs | ASan + UBSan; no window system linked |
| Render-probe options | ASan + UBSan |
| Packed material rows | ASan + UBSan |
| Version overrides | Actual CMake version logic and temporary Git fixtures |
| Model-workaround tools | Four synthetic Python tool suites |
| Pixel unpack | Real surfaceless EGL byte readback with software GL |
| Linux agent workspace | ASan + UBSan protocol/session tests; Python staging, client and process-lifecycle tests |

The complete gate has a 15-minute deadline and a 5-minute limit per outer
command; individual existing runners retain their shorter internal limits.
It stops on the first failure and returns nonzero. `summary.json` distinguishes
setup failure from group failure, timeout, interruption, and groups not run;
checkpoints can show a running group if execution is interrupted externally.
It also records commands, exit codes, durations, compiler versions, checkout
SHA/status, and sanitizer settings. Separate stdout/stderr files retain the failed group's
diagnostics. Temporary executables and synthetic pixels are cleaned up. The
capture contract's JSON receipt is retained without its pixel file.

The `Fork host regressions` workflow runs on pull requests to main, main pushes,
and manual dispatch. It has a 20-minute job limit, read-only repository
permissions, and cancels superseded runs on the same PR/ref. The existing
Windows fidelity workflow invokes the same 15-group runner before cross
compilation. Its architecture, import-closure, and packaging checks remain
separate. Branch protection is a maintainer setting and is not changed here.

## Evidence collection

```sh
python3 -B src/Tests/run_host_regressions.py collect \
  --host-results /tmp/openjk-host-results \
  --output /tmp/openjk-host-evidence
```

Collection copies only explicitly named UTF-8 text/JSON files, rejects symlinks
and binary contents, and records file hashes and missing records in
`manifest.json`. CI collects under `always()` and uploads the resulting dedicated
evidence directory for 14 days only if validation of the allowlist succeeds.
An early setup failure is reported as a gate that did not run. A failure before
checkout or loss of the runner can prevent collection; the Actions log remains
the source of evidence in that case.

The Windows workflow also allows named source/toolchain records, PE architecture
and import reports, and package file hashes. It excludes executables, DLLs,
libraries, images, game data, raw submodule binary patches, and the evaluation
archive. This preserves the distribution boundary in
[RETAIL_FIDELITY.md](../../RETAIL_FIDELITY.md#distribution-and-provenance-boundary).

These source-level regressions and the synthetic EGL test do not launch the
game or establish retail gameplay parity. They do not claim MotS runtime,
multiplayer-session, ordinary screenshot-key, or Windows runtime coverage.

## Test the runner itself

```sh
python3 -B src/Tests/test_host_regression_runner.py
```

These tests use harmless Python child commands to check failure/timeout
propagation, partial receipts, setup failures, and the artifact allowlist. They
do not compile or launch the engine.

## Opt-in Linux agent workspace

The additional agent group does not enable hooks in ordinary builds or require
game data. The native parser/session tests compile independently of SDL; the
Python tests use synthetic asset files and small child-process fixtures. The
host workflow also runs on `agent/**` branch pushes. See
[the workspace guide](../../docs/AGENT_WORKSPACE.md) for actual game launches,
which are separate from this asset-free gate.
