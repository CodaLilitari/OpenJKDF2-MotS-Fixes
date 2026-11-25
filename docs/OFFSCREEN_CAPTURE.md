# SDL offscreen capture

Linux posed capture can explicitly use SDL3's `offscreen` desktop OpenGL
driver. This path renders through EGL without an X11 or Wayland display. The
capture parser, camera application, composition/readback and PPM writer remain
the existing [hidden rendered capture](HIDDEN_RENDERED_CAPTURE.md) path.

## Select the backend

Use the same isolated session, capture-v1 request and game arguments described
in the hidden-capture guide. Set `OPENJKDF2_CAPTURE_BACKEND=sdl-offscreen`
alongside `OPENJKDF2_CAPTURE_REQUEST`. The request's four-line format is unchanged.

For software rendering with Mesa, set these variables only in the capture
child's environment:

```text
OPENJKDF2_CAPTURE_BACKEND=sdl-offscreen
SDL_VIDEO_DRIVER=offscreen
SDL_AUDIO_DRIVER=dummy
LIBGL_ALWAYS_SOFTWARE=1
GALLIUM_DRIVER=llvmpipe
LP_NUM_THREADS=1
MESA_SHADER_CACHE_DISABLE=true
```

Remove `DISPLAY` and `WAYLAND_DISPLAY`, and keep `HOME` and `XDG_RUNTIME_DIR`
inside the private session. SDL3 uses `SDL_VIDEO_DRIVER` and
`SDL_AUDIO_DRIVER`; older SDL2 builds used names without the middle underscore.
The RED capture runner sets both generations consistently.

Only `native`, `xvfb` and `sdl-offscreen` are accepted engine backend values.
Omission selects `native`, preserving direct capture invocation's normal SDL
selection. `xvfb` selects the `x11` driver; the launcher owns the display server.
Both explicit Linux modes are rejected on other platforms. Ordinary launches
ignore this capture-only backend setting. There is no automatic video-driver
fallback after an explicit request.

The engine sets the selected driver with SDL override priority before video
initialization, checks the initialized driver, checks that its GL context is
current and obtains graphics strings from that context. Offscreen also requires
an actual desktop OpenGL 3.3 or later context and loaded buffer, shader,
framebuffer, vertex-array, texture-unit and blending functions. With a
GLX-enabled GLEW build, an EGL
context may return `GLEW_ERROR_NO_GLX_DISPLAY` after successfully loading GL;
that status is accepted only for Linux offscreen capture with GL3.3 available.
Other loader errors fail. Ordinary launch loader behavior is preserved.

## Observed graphics receipt

The PPM sidecar adds this object when graphics observations succeed:

```json
{
  "graphics": {
    "schemaVersion": 1,
    "requestedBackend": "sdl-offscreen",
    "sdlVideoDriver": "offscreen",
    "sdlVersion": "3.4.12",
    "glVendor": "Mesa",
    "glRenderer": "llvmpipe (LLVM 19.1.7, 256 bits)",
    "glVersion": "4.5 (Core Profile) Mesa 25.0.7-2+deb13u1"
  }
}
```

These example values were observed in the asset-free graphics test. They are
not fixed engine requirements or an independent attestation of the driver.
Driver/version strings are nonempty printable ASCII of at most 31 bytes; each
GL string is at most 255 bytes. SDL version is the runtime version in
`major.minor.micro` form. Quotes and backslashes are escaped in JSON.

Failed observations invalidate previous graphics state. Explicit `xvfb` or
`sdl-offscreen` capture cannot write an image without a valid matching report.
The RED runner requires this report for offscreen completion; legacy runner
paths retain their optional-sidecar behavior. Engine reports do not establish
effective camera agreement, runtime resource identities or deterministic
simulation. Those acceptance checks remain separate.

## Reproduce the graphics check

The normal capture contract harness remains free of SDL and display servers:

```sh
python3 -B src/Tests/run_capture_test.py --output /tmp/capture-contract
```

It exercises backend admission, SDL boundary failure paths through stubs,
graphics string bounds and JSON escaping, stale report rejection, ordinary
launch controls, and the existing parser, UI and image behavior. The non-Linux
admission case undefines the Linux compile macro; it is not a Windows build.
ASan and UBSan remain enabled. In environments where LeakSanitizer explicitly
fails because tracing is active, run with `ASAN_OPTIONS=detect_leaks=0` and
record that leak checking was unavailable.

The real graphics test takes an explicit dependency prefix with SDL3/GLEW
headers and static libraries; system EGL/GL/Mesa runtimes are also required:

```sh
python3 -B src/Tests/run_capture_offscreen_test.py \
  --graphics-prefix /absolute/dependency-prefix \
  --output /tmp/capture-offscreen
```

The tested SDL source pin is the engine's `lib/SDL` gitlink for release 3.4.12,
`f87239e71e42da91ca317a12eefb82cfbf3393eb`. It includes SDL's offscreen/EGL
backend. The check uses the production driver initialization, current-context
observation, GL loader policy and image/receipt writer. It compiles a GLSL 3.30
shader, draws four color regions, verifies all 4,096 RGB pixels, checks PPM row
orientation and rejects controls with drawing removed, a missing shader loader
entry point, and an actual OpenGL 3.2 context. It also starts with
conflicting SDL environment hints to verify the explicit backend wins.

The test creates no game world and does not compile the full Window translation
unit. Its receipt labels those limits and records source, dependency, executable
and output hashes. A full engine build and owned-map capture remain separate
checks. No original game assets are included or needed by this graphics test.
