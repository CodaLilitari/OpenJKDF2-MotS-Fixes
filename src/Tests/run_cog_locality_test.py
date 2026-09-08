#!/usr/bin/env python3
"""Test current COG binding/dispatch code with a native GCC or Clang compiler.

Run from any directory:
    python3 /path/to/OpenJKDF2-MotS-Fixes/src/Tests/run_cog_locality_test.py --cc gcc --ubsan

Only original test code is checked in. Production fragments, the old-condition
mutant, and binaries are generated in a temporary directory and removed on exit.
No engine build, submodules, third-party Python packages, or game assets are
required. The C harness documents its stubbed executor/transport and other limits.
"""

import argparse
import hashlib
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile


REPO_ROOT = Path(__file__).resolve().parents[2]
SOURCE_PATHS = (
    "src/World/sithThing.c",
    "src/Cog/sithCog.c",
    "src/types_enums.h",
    "src/types.h",
)


def unique_match(pattern, text, description):
    matches = list(re.finditer(pattern, text, re.MULTILINE))
    if len(matches) != 1:
        raise RuntimeError(
            f"Expected one {description}; found {len(matches)}. "
            "Review the extractor against the current production source."
        )
    return matches[0]


def extract_block(text, marker, description):
    """Include the complete brace-delimited body, ignoring comments/literals."""
    match = unique_match(marker, text, description)
    opening = text.index("{", match.start())
    tokens = re.finditer(
        r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[{}]',
        text[opening:],
        re.DOTALL,
    )
    depth = 0
    for token in tokens:
        if token.group() == "{":
            depth += 1
        elif token.group() == "}":
            depth -= 1
            if depth == 0:
                return text[match.start():opening + token.end()]
    raise RuntimeError(f"Unbalanced production body: {description}")


def write_fragment(directory, name, text):
    with (directory / name).open("w", encoding="utf-8", newline="\n") as output:
        output.write(text + "\n")


def extract_production(directory):
    source_bytes = {name: (REPO_ROOT / name).read_bytes() for name in SOURCE_PATHS}
    source = {
        name: data.decode("utf-8").replace("\r\n", "\n")
        for name, data in source_bytes.items()
    }

    thing = source["src/World/sithThing.c"]
    start = unique_match(r"^[ \t]*case THINGPARAM_COG:", thing, "COG case")
    following = unique_match(
        r"^[ \t]*case THINGPARAM_PARTICLE:", thing, "following PARTICLE case"
    )
    if following.start() <= start.start():
        raise RuntimeError("Production COG case boundaries changed")
    case = thing[start.start():following.start()]
    write_fragment(directory, "thingparam_cog.inc", case)

    # Mutate only the condition; both dispatch bodies stay exactly as extracted.
    condition = unique_match(
        r"if\s*\(\s*!\s*\(pCog->flags\s*&\s*SITH_COG_SERVER\)\s*\)",
        case,
        "corrected MotS class-locality condition",
    )
    mutant = (
        case[:condition.start()]
        + "if (pCog->flags & SITH_COG_SERVER)"
        + case[condition.end():]
    )
    write_fragment(directory, "thingparam_cog_old_inversion.inc", mutant)

    cog = source["src/Cog/sithCog.c"]
    for name, return_type in (
        ("sithCog_SendMessage", "void"),
        ("sithCog_SendMessageEx", "cog_flex_t"),
    ):
        body = extract_block(
            cog, rf"^{return_type}\s+{name}\s*\(", f"{name} definition"
        )
        write_fragment(directory, name + ".inc", body)

    enums = source["src/types_enums.h"]
    constants = []
    for name in ("eSithCogFlag", "eSithCogMsgType", "eSithThingArg"):
        body = extract_block(enums, rf"^typedef enum {name}\b", name)
        start = enums.index(body)
        end = enums.index(";", start + len(body)) + 1
        constants.append(enums[start:end])
    for name in ("SITH_TF_CAPTURED", "MULTIMODEFLAG_COOP"):
        value = unique_match(
            rf"^[ \t]*{name}[ \t]*=[ \t]*(0x[0-9a-fA-F]+|[0-9]+)",
            enums,
            name,
        )
        constants.append("enum { " + value.group().strip() + " };")
    for typedef in ("typedef float flex32_t;", "typedef flex32_t cog_flex_t;"):
        unique_match(re.escape(typedef), source["src/types.h"], typedef)
        constants.append(typedef)
    write_fragment(directory, "production_constants.h", "\n\n".join(constants))
    shutil.copyfile(Path(__file__).with_name("cog_locality_test.c"),
                    directory / "cog_locality_test.c")
    return source_bytes


def find_compiler(requested):
    candidates = [requested] if requested else ["gcc", "clang", "cc"]
    for name in candidates:
        compiler = shutil.which(name)
        if compiler:
            return compiler
    raise RuntimeError(
        "Native GCC/Clang compiler not found. Set --cc to its executable path "
        "or add gcc/clang to PATH; do not select a cross compiler."
    )


def execute(command, directory, environment, timeout):
    return subprocess.run(
        command, cwd=directory, env=environment, capture_output=True,
        text=True, errors="replace", timeout=timeout, check=False,
    )


def require_success(result, description):
    if result.returncode != 0:
        raise RuntimeError(
            f"{description} exited {result.returncode}\n"
            f"{result.stdout}{result.stderr}"
        )
    if result.stderr:
        print(result.stderr, file=sys.stderr, end="")


def run_tests(compiler, ubsan):
    with tempfile.TemporaryDirectory(prefix="openmots-cog-locality-") as temp:
        directory = Path(temp)
        source_bytes = extract_production(directory)
        environment = os.environ.copy()
        flags = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Wno-sign-compare"]
        if ubsan:
            flags += ["-fsanitize=undefined", "-fno-sanitize-recover=all"]
            environment["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"

        version = execute([compiler, "--version"], directory, environment, 30)
        require_success(version, "Compiler version query")
        print(version.stdout.splitlines()[0])
        print("UndefinedBehaviorSanitizer:", "enabled" if ubsan else "disabled")

        for mutant in (False, True):
            name = "cog-locality-old-condition" if mutant else "cog-locality"
            binary = directory / (name + (".exe" if os.name == "nt" else ""))
            command = [compiler, *flags]
            if mutant:
                command.append("-DOLD_INVERSION")
            command += ["cog_locality_test.c", "-o", str(binary)]
            compiled = execute(command, directory, environment, 120)
            require_success(compiled, f"Compile {name}")
            result = execute([str(binary)], directory, environment, 30)
            if mutant:
                expected_failure = re.search(
                    r"FAIL default[^\n]*\.flags\s*==\s*expected_flags"
                    r"[^\n]*mots=1\b", result.stderr,
                )
                if result.returncode != 1 or not expected_failure:
                    raise RuntimeError(
                        "Old-condition mutant did not fail the expected MotS "
                        f"flag assertion (exit {result.returncode})\n"
                        f"{result.stdout}{result.stderr}"
                    )
                print("PASS old-condition mutant rejected by MotS flag assertion")
            else:
                require_success(result, "Production COG locality regression")
                print(result.stdout, end="")

        for name, data in source_bytes.items():
            if (REPO_ROOT / name).read_bytes() != data:
                raise RuntimeError(f"Production source changed during test: {name}")
        digest = hashlib.sha256(b"".join(source_bytes.values())).hexdigest()
        print(f"PASS production inputs unchanged (combined SHA-256 {digest})")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--cc", default=os.environ.get("CC"),
        help="native GCC/Clang executable (default: CC, then gcc/clang/cc on PATH)",
    )
    parser.add_argument(
        "--ubsan", action="store_true",
        help="require UndefinedBehaviorSanitizer; fail if unavailable",
    )
    args = parser.parse_args()
    try:
        run_tests(find_compiler(args.cc), args.ubsan)
    except (OSError, RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
