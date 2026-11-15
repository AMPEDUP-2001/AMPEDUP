"""Verify package integrity against its manifest (not an authenticity check)."""
import argparse
import hashlib
import json
from pathlib import Path


def verify(root):
    root = Path(root).resolve()
    manifest = json.loads((root / 'package-manifest.json').read_text(encoding='utf-8-sig'))
    if manifest['schema'] != 1:
        raise ValueError('Unsupported package manifest')
    expected = set()
    for entry in manifest['files']:
        relative = Path(entry['path'])
        if relative.is_absolute() or '..' in relative.parts or not relative.parts:
            raise ValueError('Unsafe manifest path')
        name = relative.as_posix()
        if name in expected or name == 'package-manifest.json':
            raise ValueError('Duplicate or recursive manifest entry')
        expected.add(name)
        path = (root / relative).resolve()
        if not path.is_relative_to(root) or not path.is_file():
            raise ValueError('Missing or external package file: ' + name)
        if path.stat().st_size != entry['size'] or hashlib.sha256(path.read_bytes()).hexdigest().upper() != entry['sha256'].upper():
            raise ValueError('Changed package file: ' + name)
    actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()}
    if actual != expected | {'package-manifest.json'}:
        raise ValueError('Unexpected package files')
    return len(expected)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    args = parser.parse_args()
    print(f'Verified {verify(args.package)} package files')
