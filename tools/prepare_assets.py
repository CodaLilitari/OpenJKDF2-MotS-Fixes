"""Build the editor resource manifest from the user's own Jedi Knight install.
No game assets are downloaded or copied into the source tree.
"""
from pathlib import Path
import argparse
import re
import struct
from collections import Counter, defaultdict


def material_catalog(jk):
    """Associate indexed textures with the palettes of levels that actually use them."""
    kinds, usage = {}, defaultdict(Counter)
    for folder in ('Resource', 'Episode'):
        for gob in (jk / folder).iterdir():
            if gob.suffix.lower() != '.gob' or gob.stem.lower().startswith('sithed'):
                continue
            for name, data in entries(gob):
                if name.endswith('.mat'):
                    kinds[name.rsplit('\\', 1)[-1]] = 'models' if name.startswith('3do\\') else 'level'
                if not name.endswith('.jkl'):
                    continue
                text = data.decode('latin1').replace('\r', '')
                cmp = re.search(r'World colormaps[^\n]*\n\s*0:\s*(\S+)', text, re.I)
                mats = re.search(r'World materials[^\n]*\n(.*?)\nend', text, re.I | re.S)
                faces = re.search(r'World surfaces[^\n]*\n(.*?)\nend', text, re.I | re.S)
                if not (cmp and mats and faces):
                    continue
                names = {int(i): n.lower() for i, n in re.findall(r'^\s*(\d+):\s*(\S+)', mats[1], re.M)}
                for line in faces[1].splitlines():
                    face = re.match(r'\s*\d+:\s*(-?\d+)\s+0x', line)
                    if face and int(face[1]) in names:
                        usage[names[int(face[1])]][cmp[1].lower()] += 1
    lines = ['# material kind colormap; generated from installed level surface references']
    for name in sorted(kinds):
        color = usage[name].most_common(1)[0][0] if usage[name] else 'dflt.cmp'
        lines.append(f'{name}\t{kinds[name]}\t{color}')
    (jk / 'editor-materials.tsv').write_text('\n'.join(lines) + '\n', encoding='ascii')


def entries(path):
    with path.open('rb') as stream:
        magic, version, offset = struct.unpack('<4sII', stream.read(12))
        if magic != b'GOB ' or version != 20:
            raise ValueError(f'Invalid JK GOB: {path}')
        stream.seek(offset)
        count, = struct.unpack('<I', stream.read(4))
        table = [struct.unpack('<II128s', stream.read(136)) for _ in range(count)]
        for offset, size, raw in table:
            name = raw.split(b'\0')[0].decode('ascii').lower()
            stream.seek(offset)
            yield name, stream.read(size)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('jk', type=Path, nargs='?', default=Path(__file__).resolve().parents[1] / 'JK')
    args = parser.parse_args()
    gob = next(p for p in (args.jk / 'Episode').iterdir() if p.name.lower() == 'jk1mp.gob')
    levels = dict(entries(gob))
    source = next(data for name, data in levels.items() if name.startswith('jkl\\'))
    source = source.decode('latin1').replace('\r', '')
    sections = re.split(r'(?im)^SECTION:\s*(\w+)\s*$', source)
    output = '# GeoSith resources, generated from your local JK installation.\n'
    for index in range(1, len(sections), 2):
        name, body = sections[index:index + 2]
        if name.lower() not in {'georesource', 'sectors', 'things', 'cogs'}:
            output += f'\nSECTION: {name}\n{body}\n'
    destination = args.jk / 'editor-resources.jkl'
    template_file = next((p for p in (args.jk.parent / 'master.tpl', args.jk.parent / 'mater.tpl') if p.exists()), None)
    if template_file:
        master = [line for line in template_file.read_text(encoding='latin1').splitlines() if line.strip() and not line.lstrip().startswith('#')]
        block = re.search(r'World templates[^\n]*\n(.*?)\nend', output, re.I | re.S)
        if block:
            existing = {line.split()[0].lower() for line in block[1].splitlines() if line.strip() and not line.lstrip().startswith('#')}
            added = [line for line in master if line.split()[0].lower() not in existing]
            output = output[:block.start()] + 'World templates 2048\n' + block[1] + '\n' + '\n'.join(added) + '\nend' + output[block.end():]
        for kind, capacity in [('sounds',4096),('materials',4096),('models',1024),('sprites',256),('keyframes',4096),('puppets',256),('soundclasses',256),('aiclasses',256),('scripts',512)]:
            output = re.sub(r'(World '+kind+r')\s+\d+', lambda m: m[1]+' '+str(capacity), output, flags=re.I)
        output = output.replace('SECTION: TEMPLATES', 'SECTION: COGS\nWorld cogs 128\nend\n\nSECTION: TEMPLATES')
    destination.write_bytes(output.encode('latin1'))
    material_catalog(args.jk)
    marker = args.jk / 'Resource' / 'jk_.cd'
    if not marker.exists():
        original = args.jk / 'JK_.CD'
        if not original.exists():
            raise FileNotFoundError('The JK installation is missing JK_.CD')
        marker.write_bytes(original.read_bytes())
    print(f'Prepared {destination} ({destination.stat().st_size} bytes)')


if __name__ == '__main__':
    main()
