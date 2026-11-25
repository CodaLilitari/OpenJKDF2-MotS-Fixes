#!/usr/bin/env python3
"""Compile the production floor-cache, plane-test and mesh-loop functions with a synthetic floor."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    pattern = r'^(?:static )?(?:int|void) ' + name + r'\([^;{}]*?\)\n\{.*?\n\}'
    match = re.search(pattern, source, flags=re.M | re.S)
    if not match:
        raise ValueError(f'Could not find the implementation of {name}')
    return match.group()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default=os.environ.get('CC', 'cc'))
    parser.add_argument('--sanitize', action='store_true', help='Enable address and undefined-behavior checks')
    args = parser.parse_args()
    src = ROOT / 'src'
    intersect = (src / 'Engine/sithIntersect.c').read_text()
    models = (src / 'Primitives/rdModel3.c').read_text()
    functions = [
        'sithIntersect_CloseFloorMissCache', 'sithIntersect_OpenFloorMissCache',
        'sithIntersect_AdvanceFloorMissCache', 'sithIntersect_IsCacheableFloorQuery',
        'sithIntersect_MakeFloorKey', 'sithIntersect_CheckSphereHit',
        'sithIntersect_CheckFaceTrackingWrites', 'sithIntersect_CheckMeshTrackingWrites',
        'sithIntersect_CheckSphereMeshIntersection', 'sithIntersect_CheckCachedFloorMesh',
    ]
    test_data = '\n\n'.join(function(intersect, name) for name in functions)
    test_data += '\n\n' + function(models, 'rdModel3_ReplaceMesh')
    with tempfile.TemporaryDirectory(prefix='openjkdf2-floor-cache-') as tmp:
        tmp = Path(tmp)
        (tmp / 'floor_production.inc').write_text(test_data)
        command = [args.cc, '-std=c11', '-O2', '-D_POSIX_C_SOURCE=200809L',
                   '-DPLATFORM_POSIX', '-DPLAT_MISSING_WIN32', '-DNO_JK_MMAP',
                   '-DSTDSOUND_NULL', '-DQOL_IMPROVEMENTS', '-I', str(src), '-I', str(tmp),
                   str(src / 'Tests/floor_miss_cache_test.c'), '-lm', '-o', str(tmp / 'test')]
        if args.sanitize:
            command[1:1] = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
        subprocess.run(command, check=True)
        subprocess.run([str(tmp / 'test')], check=True)


if __name__ == '__main__':
    main()
