#!/usr/bin/env python3
"""Compile capture contract/UI-stub tests; no SDL, game, or desktop API is linked."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def run(args, **kwargs):
    return subprocess.run(args, check=True, text=True, capture_output=True, timeout=90, **kwargs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    compiler = os.environ.get("CC", "gcc")
    flags = ["-std=c11", "-D_POSIX_C_SOURCE=200809L", "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g", "-Wall", "-Wextra", "-Werror"]
    logs = []
    with tempfile.TemporaryDirectory(prefix="openjk-capture-test-") as tmp:
        stage = Path(tmp)
        (stage / "resource").mkdir()
        (stage / "resource/jk_.cd").write_bytes(b"test")
        (stage / ".openjkdf2-capture-session").write_text("synthetic\n")
        request = stage / "request.txt"
        request.write_text("sed-engine-capture-v1\n64 64 0 3 1000\n0 0 0 0 0 0 75\n" + str(stage / "unused.ppm") + "\n")
        for name in ("capture_request_test", "capture_ui_test", "capture_launch_test", "capture_console_test", "capture_backend_test", "capture_graphics_test", "capture_sdl_test", "capture_gl_loader_test"):
            exe = stage / name
            run([compiler, *flags, "-I", str(ROOT), str(ROOT / "Tests" / (name + ".c")), "-lm", "-o", str(exe)])
            command = [str(exe)]
            if name == "capture_request_test":
                command.append(str(stage / "pixels.ppm"))
            if name in ("capture_launch_test", "capture_backend_test", "capture_sdl_test"):
                command.append(str(request))
            if name == "capture_graphics_test":
                command += [str(request), str(stage / "graphics.ppm")]
            result = run(command, cwd=stage)
            logs.append({"test": name, "stdout": result.stdout, "stderr": result.stderr})
        # Restore an unsafe RaiseWindow path: the same wrapper test must fail.
        header = ROOT / "Platform/SDL2/stdCaptureUi.h"
        altered = header.read_text().replace(
            '#define SDL_RaiseWindow(w) (stdCapture_UiSuppressed() ? false : SDL_RaiseWindow(w))',
            '#define SDL_RaiseWindow(w) SDL_RaiseWindow(w)')
        if altered == header.read_text():
            raise RuntimeError("Negative control no longer matches production source")
        local_header = stage / "Platform/SDL2/stdCaptureUi.h"
        local_header.parent.mkdir(parents=True)
        local_header.write_text(altered)
        exe = stage / "unsafe-ui"
        run([compiler, *flags, "-I", str(stage), "-I", str(ROOT), str(ROOT / "Tests/capture_ui_test.c"), "-o", str(exe)])
        failed = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
        if failed.returncode == 0 or "calls == 9" not in failed.stderr:
            raise RuntimeError("Unsafe RaiseWindow negative control did not fail the expected assertion")
        pixels = stage / "pixels.ppm"
        (args.output / "pixels.ppm").write_bytes(pixels.read_bytes())
        (args.output / "pixels.ppm.json").write_bytes((stage / "pixels.ppm.json").read_bytes())
        graphics_receipt = json.loads((stage / "graphics.ppm.json").read_text())
        assert graphics_receipt["graphics"] == {
            "schemaVersion": 1, "requestedBackend": "sdl-offscreen",
            "sdlVideoDriver": "offscreen", "sdlVersion": "3.4.12",
            "glVendor": "Mesa", "glRenderer": 'quoted "GPU" \\ test', "glVersion": "4.5"}
        (args.output / "graphics.ppm.json").write_bytes((stage / "graphics.ppm.json").read_bytes())
        # Compile the same parser as a non-Linux target: no Linux backend may
        # become active, while native and ordinary invocation still work.
        exe = stage / "capture-backend-nonlinux"
        run([compiler, *flags, "-U__linux__", "-I", str(ROOT), str(ROOT / "Tests/capture_backend_test.c"), "-lm", "-o", str(exe)])
        result = run([str(exe), str(request)], cwd=stage)
        logs.append({"test": "capture_backend_nonlinux", "stdout": result.stdout, "stderr": result.stderr})
    sources = ["Main/stdCapture.c", "Main/stdCapture.h", "Main/stdAutostartCharacter.h", "Platform/SDL2/stdCaptureUi.h", "Platform/SDL2/stdCaptureGraphics.h", "Platform/SDL2/stdCaptureGlLoader.h", "Tests/capture_request_test.c", "Tests/capture_ui_test.c", "Tests/capture_launch_test.c", "Tests/capture_console_test.c", "Tests/capture_backend_test.c", "Tests/capture_graphics_test.c", "Tests/capture_sdl_test.c", "Tests/capture_gl_loader_test.c", "Tests/run_capture_test.py"]
    receipt = {
        "schemaVersion": 1, "gameLaunched": False, "windowSystemLinked": False,
        "sanitizers": ["address", "undefined"], "testGroupsPassed": len(logs),
        "ordinaryModeControlPassed": True, "negativeControlsRejected": 1,
        "compiler": run([compiler, "--version"]).stdout.splitlines()[0],
        "sources": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in sources},
        "logs": logs,
    }
    (args.output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
