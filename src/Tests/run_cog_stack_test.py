#!/usr/bin/env python3
"""Run bounded production COG stack checks without assets or an engine build.

Usage: python3 src/Tests/run_cog_stack_test.py --cc gcc --sanitize
Fixed/dynamic and compressed/uncompressed layouts use eight test slots. The
dynamic TWL allocation path is also checked with owned host-memory stubs.
No old unsafe implementation is compiled or executed.
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

sys.dont_write_bytecode = True
from run_cog_locality_test import extract_block, find_compiler, unique_match

ROOT = Path(__file__).resolve().parents[2]
PATHS = ("src/Cog/sithCogExec.c", "src/types.h", "src/types_enums.h",
         "src/stdPlatform.c", "src/stdPlatform.h")


def prepare(directory):
    originals = {name: (ROOT / name).read_bytes() for name in PATHS}
    sources = {name: value.decode("utf-8").replace("\r\n", "\n")
               for name, value in originals.items()}
    types = sources["src/types.h"]
    symbols = unique_match(
        r"^#ifdef COG_COMPRESS_VAR_SIZE\n(?s:.*?)^#endif(?=\n\ntypedef struct sithCog\n)",
        types, "symbol representations").group()
    fields = unique_match(
        r"^#ifdef COG_DYNAMIC_STACKS\n    SithCogSymbolValue\* stack;\n(?s:.*?)^#endif\n    uint32_t stackPos;",
        types, "stack storage fields").group()
    declarations = ["typedef struct sithCog sithCog;"]
    for declaration in ("typedef int32_t cog_int_t;", "typedef float flex32_t;",
                        "typedef flex32_t cog_flex_t;", "typedef void (*cogSymbolFunc_t)(sithCog *);"):
        declarations.append(unique_match(re.escape(declaration), types, declaration).group())
    value = unique_match(r"^[ \t]*SITHCOG_VALUE_INT\s*=\s*\d+",
                         sources["src/types_enums.h"], "integer symbol type").group()
    declarations += ["enum { " + value.strip() + " };", symbols,
                     "struct sithCog {\n" + fields + "\n};"]
    (directory / "stack_types.inc").write_text("\n".join(declarations) + "\n")
    code = sources[PATHS[0]]
    operations = []
    for name, kind in (("sithCogExec_PushStack", "void"), ("sithCogExec_PopStack", "int32_t"),
                       ("sithCogExec_ResetStack", "void")):
        operations.append(extract_block(code, rf"^{kind}\s+{name}\s*\(", name))
    operations += ["#ifdef COG_DYNAMIC_STACKS", extract_block(
        code, r"^void\s+sithCogExec_GrowStack\s*\(", "stack growth"), "#endif"]
    (directory / "stack_operations.inc").write_text("\n\n".join(operations) + "\n")
    word = extract_block(sources["src/stdPlatform.h"],
                         r"^static inline void stdPlatform_WriteByte16\(", "word byte store")
    copy = extract_block(sources["src/stdPlatform.c"],
                         r"^void stdPlatform_Memcpy32\(", "word copy")
    (directory / "word_copy.inc").write_text(
        word + "\n#define stdPlatform_Memcpy32 testWordCopy\n" + copy +
        "\n#undef stdPlatform_Memcpy32\n")
    shutil.copyfile(Path(__file__).with_name("cog_stack_test.c"), directory / "cog_stack_test.c")
    return originals


def run(compiler, sanitize):
    flags = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-fno-strict-aliasing"]
    if sanitize:
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    env = os.environ.copy()
    env.setdefault("UBSAN_OPTIONS", "halt_on_error=1:print_stacktrace=1")
    print(subprocess.check_output([compiler, "--version"], text=True).splitlines()[0], flush=True)
    with tempfile.TemporaryDirectory(prefix="openjk-cog-stack-") as temporary:
        directory = Path(temporary)
        originals = prepare(directory)
        for dynamic, compressed, twl in ((False, False, False), (False, True, False),
                                         (True, False, False), (True, True, False),
                                         (True, False, True), (True, True, True)):
            label = f"{'dynamic' if dynamic else 'fixed'}-{'compressed' if compressed else 'full'}"
            if twl:
                label += "-twl-allocation"
            defines = []
            for enabled, name in ((dynamic, "COG_DYNAMIC_STACKS"),
                                  (compressed, "COG_COMPRESS_VAR_SIZE"), (twl, "TARGET_TWL")):
                if enabled:
                    defines.append("-D" + name)
            binary = directory / (label + (".exe" if os.name == "nt" else ""))
            subprocess.run([compiler, *flags, *defines, "cog_stack_test.c", "-o", str(binary)],
                           cwd=directory, env=env, check=True, timeout=60)
            result = subprocess.run([str(binary)], cwd=directory, env=env, text=True,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
            if result.returncode:
                raise RuntimeError(f"{label} failed ({result.returncode})\n{result.stdout}{result.stderr}")
            if result.stderr:
                print(result.stderr, file=sys.stderr, end="")
            print(label + ": " + result.stdout.strip(), flush=True)
        for name, content in originals.items():
            if (ROOT / name).read_bytes() != content:
                raise RuntimeError(f"Production input changed during the test: {name}")
            print(f"SHA256 {hashlib.sha256(content).hexdigest()} {name}")
        print("PASS all six layouts; production inputs unchanged; no old-path execution")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cc", default=os.environ.get("CC"))
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    run(find_compiler(args.cc), args.sanitize)
