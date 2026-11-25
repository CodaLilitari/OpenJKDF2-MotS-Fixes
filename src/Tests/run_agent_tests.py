#!/usr/bin/env python3
"""Exercise the actual Linux agent protocol/session code without SDL or assets."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
    parser.add_argument("--output", type=Path)
    parser.add_argument("--no-sanitizers", action="store_true")
    args = parser.parse_args()
    if platform.system() != "Linux":
        print(json.dumps({"schemaVersion": 1, "skipped": "Linux-only local agent hooks"}))
        return
    flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-g"]
    if not args.no_sanitizers:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    tests = [
        ("agent_protocol_test", [], ["Main/stdAgentProtocol.c"]),
        ("agent_protocol_test", ["lease"], ["Main/stdAgentProtocol.c"]),
        ("agent_session_test", [], ["Main/stdAgentProtocol.c", "Main/stdAgentSession.c"]),
    ]
    logs = []
    with tempfile.TemporaryDirectory(prefix="openjk-agent-tests-") as directory:
        compiled = set()
        for name, arguments, sources in tests:
            executable = Path(directory) / name
            if name not in compiled:
                subprocess.run(
                    [args.cc, *flags, "-I", str(ROOT), str(ROOT / "Tests" / f"{name}.c"),
                     *[str(ROOT / source) for source in sources], "-o", str(executable)],
                    check=True, timeout=90,
                )
                compiled.add(name)
            completed = subprocess.run([str(executable), *arguments], text=True,
                                       capture_output=True, timeout=15)
            if completed.returncode:
                sys.stderr.write(completed.stdout + completed.stderr)
                completed.check_returncode()
            logs.append({"test": " ".join([name, *arguments]), "stdout": completed.stdout,
                         "stderr": completed.stderr})
    sources = ["Main/stdAgentProtocol.c", "Main/stdAgentProtocol.h", "Main/stdAgentSession.c",
               "Main/stdAgentSession.h", "Tests/agent_protocol_test.c", "Tests/agent_session_test.c"]
    receipt = {"schemaVersion": 1, "gameLaunched": False, "windowSystemLinked": False,
               "testGroupsPassed": len(logs), "sanitizers": [] if args.no_sanitizers else ["address", "undefined"],
               "sources": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in sources},
               "logs": logs}
    if args.output:
        args.output.mkdir(parents=True, exist_ok=True)
        (args.output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(receipt, indent=2))


if __name__ == "__main__":
    main()
