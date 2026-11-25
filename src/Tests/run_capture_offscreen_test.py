#!/usr/bin/env python3
"""Build/run the asset-free production capture boundary with SDL3 + desktop GL.

Supply a dependency prefix containing include/SDL3, include/GL, lib/libSDL3.a,
and lib/libGLEW.a. System GL/EGL/Mesa runtimes remain outside that prefix.
This does not build or launch the full engine, load a map, or observe a camera.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--graphics-prefix", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output, prefix = args.output.resolve(), args.graphics_prefix.resolve()
    output.mkdir(parents=True, exist_ok=False)
    stage = output / "stage"
    (stage / "resource").mkdir(parents=True)
    (stage / "resource/jk_.cd").write_bytes(b"test")
    (stage / ".openjkdf2-capture-session").write_text("synthetic graphics test\n")
    request = stage / "request.txt"
    request.write_text("sed-engine-capture-v1\n64 64 0 1 1000\n0 0 0 0 0 0 75\n" + str(output / "capture.ppm") + "\n")
    env = dict(os.environ)
    for key in ("DISPLAY", "WAYLAND_DISPLAY", "MESA_GL_VERSION_OVERRIDE", "MESA_GLSL_VERSION_OVERRIDE"):
        env.pop(key, None)
    env.update(SDL_VIDEO_DRIVER="x11,wayland", SDL_VIDEODRIVER="x11",
               SDL_AUDIO_DRIVER="dummy", SDL_AUDIODRIVER="dummy",
               LIBGL_ALWAYS_SOFTWARE="1", GALLIUM_DRIVER="llvmpipe", LP_NUM_THREADS="1",
               MESA_SHADER_CACHE_DISABLE="true", HOME=str(stage), XDG_RUNTIME_DIR=str(stage))
    source = ROOT / "Tests/capture_offscreen_gl_test.c"
    libraries = [prefix / "lib/libGLEW.a", prefix / "lib/libSDL3.a"]
    command = [os.environ.get("CC", "gcc"), "-std=c11", "-D_POSIX_C_SOURCE=200809L",
               "-Wall", "-Wextra", "-Werror", "-I", str(ROOT), "-I", str(prefix / "include"),
               str(source), *map(str, libraries), "-l:libGL.so.1", "-pthread", "-lm", "-ldl"]
    logs = []
    cases = [
        ("offscreen-draw", [], None),
        ("negative-draw", ["-DSTDCAPTURE_NEGATIVE_DRAW"], "pixels[offset]"),
        ("missing-shader", ["-DSTDCAPTURE_MISSING_SHADER", "-Wl,--wrap=glXGetProcAddressARB"], "stdCapture_InitGlLoader(&loader)"),
        ("old-context", ["-DSTDCAPTURE_GL32_CONTEXT"], "stdCapture_InitGlLoader(&loader)"),
    ]
    for name, flags, expected_failure in cases:
        executable = output / name
        build = subprocess.run([*command, *flags, "-o", str(executable)],
                               check=True, capture_output=True, text=True, timeout=90)
        (output / (name + "-build.log")).write_text(build.stdout + build.stderr)
        child_env = dict(env)
        if name == "old-context":
            child_env.update(MESA_GL_VERSION_OVERRIDE="3.2", MESA_GLSL_VERSION_OVERRIDE="150")
        result = subprocess.run([str(executable), str(request)], cwd=stage, env=child_env,
                                capture_output=True, text=True, timeout=30)
        (output / (name + ".log")).write_text(result.stdout + result.stderr)
        if expected_failure:
            if result.returncode == 0 or expected_failure not in result.stderr:
                raise RuntimeError(f"{name} did not fail the expected assertion: {result.stderr}")
        elif result.returncode:
            raise RuntimeError(f"Offscreen test failed ({result.returncode}): {result.stderr}")
        logs.append({"name": name, "exitCode": result.returncode,
                     "executableSha256": digest(executable), "stdout": result.stdout, "stderr": result.stderr})
    data = (output / "capture.ppm").read_bytes()
    assert data[:13] == b"P6\n64 64\n255\n" and len(data) == 13 + 64*64*3
    # The production writer flips rows into top-first PPM ordering.
    expected = bytes(channel for y in range(63, -1, -1) for x in range(64)
                     for channel in (255 if x < 32 else 0, 255 if y < 32 else 0, 255 if x >= 32 else 0))
    assert data[13:] == expected
    sidecar = json.loads((output / "capture.ppm.json").read_text())
    graphics = sidecar["graphics"]
    assert graphics["schemaVersion"] == 1 and graphics["requestedBackend"] == "sdl-offscreen"
    assert graphics["sdlVideoDriver"] == "offscreen"
    assert sidecar["engineWorldStateAvailable"] is False
    sources = [source, ROOT / "Main/stdCapture.c", ROOT / "Main/stdCapture.h",
               ROOT / "Platform/SDL2/stdCaptureGraphics.h", ROOT / "Platform/SDL2/stdCaptureGlLoader.h", Path(__file__).resolve()]
    receipt = {"schemaVersion": 1, "gameLaunched": False, "fullEngineBuilt": False,
               "windowSystemLinked": True, "displayServerUsed": False,
               "pixelsVerified": 4096, "shaderDrawVerified": True, "negativeControlsRejected": 3,
               "graphics": graphics, "imageSha256": digest(output / "capture.ppm"),
               "engineReceiptSha256": digest(output / "capture.ppm.json"),
               "sourceHashes": {str(p.relative_to(ROOT)): digest(p) for p in sources},
               "dependencyHashes": {p.name: digest(p) for p in libraries}, "logs": logs}
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
