#!/usr/bin/env python3
"""Exercise the actual bitmap conversion function and its removed-guard mutant.

Requires GCC or Clang with UndefinedBehaviorSanitizer. No renderer or game data
is used; bitmap_test.c documents its dependency stubs. All generated files live
in a temporary directory. Run: python3 src/Tests/run_bitmap_test.py --cc gcc
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True


def run(repo, compiler):
    helper_path = repo / 'src/Tests/run_cog_locality_test.py'
    spec = importlib.util.spec_from_file_location('bitmap_extraction', helper_path)
    helper = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(helper)
    names = ('src/General/stdBitmap.c', 'src/General/stdBitmap.h', 'src/types.h')
    inputs = {name: (repo / name).read_bytes() for name in names}
    source = {name: value.decode('utf-8').replace('\r\n', '\n') for name, value in inputs.items()}
    function = helper.extract_block(source[names[0]], r'^void stdBitmap_ConvertColorFormat\(', 'bitmap conversion function')
    guard = '\n    if (!bitmap) return;\n'
    if function.count(guard) != 1:
        raise RuntimeError('Expected exactly one reviewed null-bitmap guard')
    types = []
    for path, name in ((names[2], 'rdTexFormat'), (names[1], 'stdBitmap')):
        value = helper.extract_block(source[path], r'^typedef struct ' + name + r'\s*\{', name)
        types.append(value + ' ' + name + ';')
    type_source = '#include <stdint.h>\n' + types[0] + '\ntypedef struct tVBuffer tVBuffer;\n' + types[1] + '\n'
    with tempfile.TemporaryDirectory(prefix='openjkdf2-bitmap-test-') as directory:
        work = Path(directory)
        for name, value in (('source_types.inc', type_source),
                            ('actual_function.inc', function + '\n'),
                            ('guard_removed_function.inc', function.replace(guard, '\n') + '\n')):
            (work / name).write_text(value, encoding='utf-8', newline='\n')
        shutil.copyfile(Path(__file__).with_name('bitmap_test.c'), work / 'bitmap_test.c')
        flags = ['-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror',
                 '-fsanitize=undefined', '-fno-sanitize-recover=undefined']
        for name, extra in (('fixed', []), ('mutant', ['-DWITHOUT_GUARD'])):
            command = [compiler, *flags, *extra, 'bitmap_test.c', '-o', str(work / name)]
            subprocess.run(command, cwd=work, check=True, capture_output=True, text=True, timeout=120)
        positive = subprocess.run([str(work / 'fixed')], cwd=work, capture_output=True, text=True, timeout=30)
        if positive.returncode or not positive.stdout.startswith('PASS actual stdBitmap_ConvertColorFormat:'):
            raise RuntimeError('Bitmap regression failed:\n' + positive.stdout + positive.stderr)
        negative = subprocess.run([str(work / 'mutant'), 'null'], cwd=work, capture_output=True, text=True, timeout=30)
        if negative.returncode == 0 or 'runtime error:' not in negative.stderr or 'null pointer' not in negative.stderr:
            raise RuntimeError('Guard-removal mutant did not produce the expected UBSan null-input failure:\n' + negative.stdout + negative.stderr)
        if any((repo / name).read_bytes() != value for name, value in inputs.items()):
            raise RuntimeError('Production source changed during the regression run')
        print(positive.stdout.strip())
        print('Removed-guard mutant failed with the expected UBSan null-pointer diagnostic.')
        print(json.dumps({'source_sha256': {name: hashlib.sha256(value).hexdigest() for name, value in inputs.items()},
                          'compiler': subprocess.check_output([compiler, '--version'], text=True).splitlines()[0],
                          'scope': 'Actual function and source types; explicit renderer/color stubs, no game runtime.'}, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='gcc')
    parser.add_argument('--repo-root', type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    compiler = shutil.which(args.cc)
    if compiler is None:
        parser.error('Compiler not found: ' + args.cc)
    run(args.repo_root.resolve(), compiler)


if __name__ == '__main__':
    main()
