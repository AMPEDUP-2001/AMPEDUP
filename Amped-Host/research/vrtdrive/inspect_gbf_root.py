"""Inspect only the root header of observed Amped GBF class-0x100 models."""
import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'vrtdrive'))
from inspect_directory import directory, MAX_FILE


def root_header(data):
    if len(data) < 74:
        raise ValueError('truncated root header')
    version, marker, kind = struct.unpack_from('<fII', data, 0)
    if version != 2.0 or marker != 0xFF000000 or kind != 0x100:
        raise ValueError('unsupported observed root format')
    raw_name = data[12:44]
    if b'\0' not in raw_name:
        raise ValueError('unterminated root name')
    try:
        name = raw_name.split(b'\0', 1)[0].decode('ascii')
    except UnicodeDecodeError as exc:
        raise ValueError('non-ASCII root name') from exc
    # The 28 bytes have not been semantically decoded; retain raw only.
    return {'version': version, 'marker': marker, 'class': kind,
            'name': name, 'root_child_count': struct.unpack_from('<H', data, 72)[0]}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('archive', type=Path)
    p.add_argument('entry')
    p.add_argument('--tree-prefix', action='store_true', help='walk supported classes until the first unsupported record')
    a = p.parse_args()
    if a.archive.stat().st_size > MAX_FILE:
        p.error('archive exceeds size limit')
    data = a.archive.read_bytes()
    matches = [(start, size) for name, start, size in directory(data) if name.lower() == a.entry.lower()]
    if len(matches) != 1:
        p.error('entry missing or ambiguous')
    start, size = matches[0]
    payload = data[start:start + size]
    if a.tree_prefix:
        from gbf_known_prefix import known_prefix
        nodes, frontier = known_prefix(payload)
        for node in nodes: print(node)
        print('frontier:', frontier)
    else:
        print(root_header(payload))


if __name__ == '__main__': main()
