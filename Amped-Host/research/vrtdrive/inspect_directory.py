"""Read-only inspector for the observed Amped VRTDRIVE directory layout."""
import argparse
import struct
from pathlib import Path

MAX_FILE = 128 * 1024 * 1024


def directory(data):
    if len(data) < 46 or len(data) > MAX_FILE or data[:8] != b'VRTDRIVE':
        raise ValueError('invalid or oversized VRTDRIVE file')
    count = struct.unpack_from('<H', data, 10)[0]
    offset, length = struct.unpack_from('<II', data, 12)
    if offset < 36 or offset > len(data) or length != len(data) - offset - 8:
        raise ValueError('invalid directory bounds')
    if data[offset:offset + 8] != b'VDRIVDIR' or length < 2:
        raise ValueError('missing directory marker or count')
    end = len(data) - 2
    if struct.unpack_from('<H', data, end)[0] != count:
        raise ValueError('directory count mismatch')
    pos = offset + 8
    rows = []
    for _ in range(count):
        if pos >= end:
            raise ValueError('truncated entry name')
        n = data[pos]
        pos += 1
        if n == 0 or pos + n + 32 > end:
            raise ValueError('invalid entry length')
        try:
            name = data[pos:pos + n].decode('ascii')
        except UnicodeDecodeError as exc:
            raise ValueError('non-ASCII entry') from exc
        pos += n
        fields = struct.unpack_from('<8I', data, pos)
        pos += 32
        start, size = fields[1:3]
        if start < 36 or start > offset or size > offset - start:
            raise ValueError('payload outside data region')
        rows.append((name, start, size))
    if pos != end:
        raise ValueError('unexpected trailing directory data')
    spans = sorted((start, start + size) for _, start, size in rows if size)
    if any(a[1] > b[0] for a, b in zip(spans, spans[1:])):
        raise ValueError('overlapping payloads')
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('resource', type=Path)
    parser.add_argument('--contains', default='', help='case-insensitive name filter')
    args = parser.parse_args()
    if args.resource.stat().st_size > MAX_FILE:
        parser.error('file exceeds size limit')
    rows = directory(args.resource.read_bytes())
    for name, offset, size in rows:
        if args.contains.lower() in name.lower():
            print(f'{name}\t0x{offset:X}\t0x{size:X}')
    print(f'validated {len(rows)} entries')


if __name__ == '__main__':
    main()
