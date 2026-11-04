"""Import installed stock maps without modifying their archives."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess

from prepare_assets import entries


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser()
    parser.add_argument('--executable', type=Path, default=root / 'build/network-audit-run/openjkdf2-64.exe')
    parser.add_argument('--maps', nargs='*')
    args = parser.parse_args()
    env = dict(os.environ, OPENJKDF2_ROOT=str(root / 'JK'), ALSOFT_DRIVERS='null', GEOSITH_STOCK_TEST='1')
    env['PATH'] = str(root / 'build') + os.pathsep + env['PATH']
    for key in ('GEOSITH_STOCK_EXPORT', 'GEOSITH_STRESS', 'GEOSITH_NET_ROLE', 'GEOSITH_SELFTEST'):
        env.pop(key, None)
    failed = []
    for episode in ('JK1', 'JK1MP'):
        archive = root / 'JK/Episode' / (episode + '.GOB')
        before = hashlib.sha256(archive.read_bytes()).digest()
        maps = [name.rsplit('\\', 1)[-1] for name, _ in entries(archive) if name.endswith('.jkl')]
        for name in maps:
            if args.maps and name not in args.maps:
                continue
            log = root / 'build' / ('stock-' + name + '.log')
            with log.open('w') as out:
                try:
                    result = subprocess.run([str(args.executable.resolve()), '-headless', '-autostart', '-sp', '-episode', episode, '-map', name], cwd=root / 'JK', env=env, stdout=out, stderr=subprocess.STDOUT, timeout=60, creationflags=subprocess.CREATE_NO_WINDOW)
                    ok = result.returncode == 0
                except subprocess.TimeoutExpired:
                    ok = False
            text = log.read_text(errors='replace')
            ok = ok and 'GEOSITH_STOCK PASS:' in text
            print(('PASS ' if ok else 'FAIL ') + episode + '/' + name, flush=True)
            if not ok:
                failed.append(name)
        assert hashlib.sha256(archive.read_bytes()).digest() == before, 'Stock archive changed'
    if failed:
        raise SystemExit('Conversion failed: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
