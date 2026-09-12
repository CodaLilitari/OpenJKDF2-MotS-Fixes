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
        for name in ("capture_request_test", "capture_ui_test"):
            exe = stage / name
            run([compiler, *flags, "-I", str(ROOT), str(ROOT / "Tests" / (name + ".c")), "-lm", "-o", str(exe)])
            command = [str(exe)]
            if name == "capture_request_test":
                command.append(str(stage / "pixels.ppm"))
            result = run(command)
            logs.append({"test": name, "stdout": result.stdout, "stderr": result.stderr})
        # Restore an unsafe RaiseWindow path: the same wrapper test must fail.
        header = ROOT / "Platform/SDL2/stdCaptureUi.h"
        altered = header.read_text().replace(
            '#define SDL_RaiseWindow(w) (stdCapture_IsRequested() ? false : SDL_RaiseWindow(w))',
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
    sources = ["Main/stdCapture.c", "Main/stdCapture.h", "Platform/SDL2/stdCaptureUi.h", "Tests/capture_request_test.c", "Tests/capture_ui_test.c", "Tests/run_capture_test.py"]
    receipt = {
        "schemaVersion": 1, "gameLaunched": False, "windowSystemLinked": False,
        "sanitizers": ["address", "undefined"], "testGroupsPassed": 2,
        "negativeControlsRejected": 1, "compiler": run([compiler, "--version"]).stdout.splitlines()[0],
        "sources": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in sources},
        "logs": logs,
    }
    (args.output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
