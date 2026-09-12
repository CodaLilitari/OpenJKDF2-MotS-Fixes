# Hidden rendered inspection capture

Implemented and exercised on Windows x64 on 2026-09-12, starting from engine
commit `5fa4644d03080fef69d7137eaa2c8f295c47bd63`. This is an opt-in inspection
feature, not a gameplay controller, deterministic replay system, or retail-parity
certificate. Existing normal launches and `-headless` retain their meanings.

## Invocation and resource scope

Set `OPENJKDF2_CAPTURE_REQUEST` to an absolute request-file path and invoke:

```text
openjkdf2-64.exe -capture -autostart -sp -motsCompat -noHUD -episode JKM -map s1l1_rebelbase.jkl
```

The request is at most 4096 bytes, with LF line endings and this format:

```text
sed-engine-capture-v1
960 640 80 3 30000
-5.19905 5.10426 3.003014 0 180 0 98.0306723659185
C:\explicit\new-session\rebelbase.ppm
```

Line two supplies width, height, sector, capture frame, and timeout milliseconds.
Line three supplies position XYZ, pitch/yaw/roll in degrees, and **horizontal**
FOV. The last line is an absolute new `.ppm` output path. Limits are 64–2048 per
dimension, at most 2,097,152 pixels, frame 1–120, timeout 1–60 seconds, and FOV
20–140 degrees. Numbers must be finite and bounded. Episode/map arguments accept
only bounded ASCII names, with no directory traversal or additional startup
switches. The `-capture` flag without its request is rejected before SDL startup.

Use an explicitly staged, writable working directory containing
`.openjkdf2-capture-session` and `resource/jk_.cd`. Capture bypasses installation
discovery and uses that session's ordinary resources and profile. Do not put the
marker in an existing personal game installation. Profile creation and other
ordinary runtime writes stay in the staged session.

[Test-RebelBaseHiddenCapture.ps1](../src/Tests/Test-RebelBaseHiddenCapture.ps1)
is a reproducible launcher for the owned original MotS Rebel Base check. It
requires explicit baseline, engine-directory and new output-directory paths;
copies four exact resource inputs and the engine dependencies; verifies staged
hashes; launches hidden with redirected streams; and imposes an independent
45-second child-process deadline. It rechecks original and staged resource hashes
afterward. It neither modifies the baseline nor discovers other installations.

## Desktop and audio behavior

Capture keeps a GL context and creates hidden, nonfocusable windows. A shared
SDL policy applies to initial creation and recreation, error dialogs, raise,
fullscreen, relative mouse mode, mouse warps, text input, cursor changes, and URL
opens. Captures ignore keyboard/mouse/controller input. The Windows console and
crash dialog paths are suppressed, the updater check is skipped, and installer
entry points fail before opening native UI. Unexpected in-engine confirmation or
error dialogs terminate the capture with a diagnostic. The singleplayer briefing
acknowledgment wait is skipped only in capture mode.

OpenAL uses the existing headless dummy/no-device path throughout capture. MCI
music startup/play also returns before opening a backend. The render window stays
alive; this does not reuse `Main_bHeadless`, which would destroy the GL context.

The engine deadline is cooperative. It cannot interrupt a blocked driver, load,
or script between event pumps. An outer process timeout remains mandatory.

## Camera, output and evidence

The capture applies the declared position, orientation, sector and FOV after the
ordinary camera update, before scene drawing. Only frames with that explicit
camera application count. Readback occurs on the render thread after composition
and before buffer swap. The first-person weapon and HUD are disabled. Pixel-pack
state and dimensions are checked before writing.

The image is a lossless binary PPM (RGB8, top row first) and the sidecar is
`<image>.json`. Both use exclusive file creation, refuse existing paths and are
removed on ordinary write failures. A process crash can leave incomplete files;
acceptance requires exit zero, a valid sidecar and exact PPM header/data length.
The launcher verifies these conditions. It separately records binary, request
and resource hashes; the engine sidecar explicitly says that the engine itself
did not verify resource hashes.

The engine uses ordinary timed simulation and the staged profile's visual
settings. Camera pose alone does not freeze AI, animation, COGs, light pulses or
runtime RNG. Near/far clipping, pixel-center conventions and renderer effects
also remain distinct from the editor. The engine matrix's forward direction is
local +Y: pitch 0/yaw 180/roll 0 looks along world -Y. SED's camera FOV is vertical,
so its 75-degree view at 960×640 corresponds nominally to 98.0306723659185 degrees
horizontal in this engine request.

## Validation at this checkpoint

- The Windows x64 executable built in an isolated WSL source/dependency tree.
  Original submodule worktrees were not modified.
- [run_capture_test.py](../src/Tests/run_capture_test.py) compiles two test groups
  with AddressSanitizer and UndefinedBehaviorSanitizer. They exercise malformed
  requests, limits, image orientation, no-overwrite behavior, frame gating and
  actual desktop-wrapper policy using stubs; no SDL/window system is linked.
  Restoring an unsafe `RaiseWindow` call makes the negative control fail.
- The existing production MotS lighting/placed-thing harness still passes all
  **1,185 checks** with AddressSanitizer and UndefinedBehaviorSanitizer. All seven
  old-behavior controls fail as expected after the capture integration.
- An initial hidden SP attempt reached the original level but waited for its
  mission briefing until the capture deadline. The subsequent briefing-skip fix
  produced the requested original Rebel Base frame in **1.77 seconds**, exit zero,
  at **960×640**, captured frame **3**. The image was visually inspected and shows
  the expected corridor geometry, materials, lighting and distant NPC.
- The successful launch sampled owned windows nine times at approximately 100 ms
  intervals: six owned windows maximum, zero visible and zero foreground owned
  windows. Source review and these observations support the hidden behavior;
  sampling cannot exclude a transient window entirely between observations.
- All four original and staged resource hashes matched before/after. No WBW
  conversion or retail executable was involved. The result is a real OpenJKDF2
  image for comparison, not proof of editor or retail pixel parity.

Ignored evidence lives under `build_win64/capture-audit/`, including the successful
`retail-05/launcher-receipt.json`, PPM and byte-faithful PNG conversion. Source,
test and build hashes are recorded separately so a later build is not confused
with this validated executable.
