"""Partial read-only GBF tree walk; stop at the first unsupported class."""
import struct
from inspect_gbf_root import root_header


def known_prefix(data, max_nodes=128, max_depth=32):
    root = root_header(data)
    nodes = [{'depth': 0, 'class': 0x100, 'name': root['name'],
              'child_count': root['root_child_count']}]
    pos = 74

    def walk(depth):
        nonlocal pos
        if depth > max_depth or len(nodes) >= max_nodes:
            raise ValueError('node or depth limit')
        if len(data) - pos < 4:
            raise ValueError('truncated class')
        kind = struct.unpack_from('<I', data, pos)[0]
        if kind not in (0x100, 0x2000, 0x20000, 0x02000000, 0x80000000):
            return {'offset': pos, 'class': kind}
        start = pos
        custom = kind == 0x80000000
        if custom:
            if len(data) - pos < 16: raise ValueError('truncated custom class')
            if data[pos + 4:pos + 8] not in (b'EXTN', b'TEXT'):
                return {'offset': pos, 'class': kind}
        subtype = data[pos + 4:pos + 8] if custom else None
        prefix = 16 if custom else 4
        extra = 28 if kind in (0x100, 0x80000000) else 12 if kind == 0x2000 else 0
        leaf = kind == 0x20000
        if len(data) - pos < prefix + 32 + extra + (0 if leaf else 2):
            raise ValueError('truncated known-class prefix')
        name_raw = data[pos + prefix:pos + prefix + 32]
        if b'\0' not in name_raw:
            raise ValueError('unterminated name')
        try: name = name_raw.split(b'\0', 1)[0].decode('ascii')
        except UnicodeDecodeError as exc: raise ValueError('non-ASCII name') from exc
        pos += prefix + 32 + extra
        external = None
        if custom:
            if len(data) - pos < 4: raise ValueError('truncated external length')
            length = struct.unpack_from('<I', data, pos)[0]
            pos += 4
            if length > 1024 or len(data) - pos < length + 2:
                raise ValueError('invalid external length')
            try: external = data[pos:pos + length].decode('ascii')
            except UnicodeDecodeError as exc: raise ValueError('non-ASCII external name') from exc
            pos += length
        if subtype == b'TEXT':
            # flags (2), two 4-byte values, then two arrays of 12-byte items.
            if len(data) - pos < 10: raise ValueError('truncated text fields')
            pos += 10
            for _ in range(2):
                if len(data) - pos < 2: raise ValueError('truncated text array count')
                count = struct.unpack_from('<H', data, pos)[0]
                pos += 2
                if count > 4096 or len(data) - pos < count * 12:
                    raise ValueError('invalid text array size')
                pos += count * 12
        child_count = 0 if leaf else struct.unpack_from('<H', data, pos)[0]
        if not leaf: pos += 2
        node = {'offset': start, 'depth': depth, 'class': kind,
                'name': name, 'child_count': child_count}
        if custom: node['subtype'] = subtype.decode('ascii'); node['payload_string'] = external
        nodes.append(node)
        for _ in range(child_count):
            frontier = walk(depth + 1)
            if frontier is not None: return frontier
        return None

    for _ in range(root['root_child_count']):
        frontier = walk(1)
        if frontier is not None: return nodes, frontier
    return nodes, None
