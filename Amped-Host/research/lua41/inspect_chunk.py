"""Read-only, bounded inspector for the 32-bit little-endian Lua 4.1 alpha format."""
import argparse
import struct
from pathlib import Path

HEADER = b'\x1bLua\x41\x01\x04\x04\x04\x06\x08\x08\x0a\x08'
TEST_NUMBER = 3.14159265358979323846e8
OPS = 'MOVE LOADK LOADINT LOADNIL LOADUPVAL GETGLOBAL GETTABLE SETGLOBAL SETTABLE NEWTABLE SELF ADD SUB MUL DIV POW UNM NOT CONCAT JMP CJMP TESTEQ TESTNE TESTLT TESTLE TESTGT TESTGE TESTT TESTF NILJMP CALL RETURN FORPREP FORLOOP TFORPREP TFORLOOP SETLIST SETLISTO CLOSURE'.split()
MAX_VECTOR, MAX_DEPTH, MAX_FUNCTIONS, MAX_STRING = 100000, 64, 10000, 1000000


class FormatError(ValueError):
    pass


class Reader:
    def __init__(self, data, offset=0):
        if offset < 0 or offset > len(data):
            raise FormatError('offset outside input')
        self.data, self.pos, self.functions = data, offset, []

    def take(self, n):
        if n < 0 or n > len(self.data) - self.pos:
            raise FormatError(f'truncated data at 0x{self.pos:x}')
        result = self.data[self.pos:self.pos+n]
        self.pos += n
        return result

    def uint(self):
        return struct.unpack('<I', self.take(4))[0]

    def count(self):
        n = self.uint()
        if n > MAX_VECTOR:
            raise FormatError(f'vector too large: {n}')
        return n

    def string(self):
        n = self.uint()
        if n > MAX_STRING:
            raise FormatError(f'string too large: {n}')
        if not n:
            return None
        b = self.take(n)
        if b[-1] != 0:
            raise FormatError('string lacks terminator')
        return b[:-1].decode('latin-1')

    def function(self, parent=None, depth=0):
        if depth > MAX_DEPTH or len(self.functions) >= MAX_FUNCTIONS:
            raise FormatError('prototype limit exceeded')
        source = self.string() or parent
        line = self.uint()
        stack_size_at = self.pos + 12
        meta = tuple(self.uint() for _ in range(4))
        for _ in range(self.count()):
            self.string()
            self.take(8)
        self.take(4 * self.count())
        constants = []
        constant_count_at = self.pos
        for _ in range(self.count()):
            tag = self.take(1)[0]
            if tag == 2:
                constants.append(struct.unpack('<d', self.take(8))[0])
            elif tag == 3:
                constants.append(self.string())
            else:
                raise FormatError(f'unsupported constant type {tag}')
        fn = {'source': source, 'line': line, 'meta': meta, 'constants': constants,
              'constant_count_at': constant_count_at, 'constant_end': self.pos,
              'stack_size_at': stack_size_at,
              'children': [], 'code': []}
        index = len(self.functions)
        self.functions.append(fn)
        for _ in range(self.count()):
            fn['children'].append(self.function(source, depth + 1))
        fn['code_count_at'] = self.pos
        fn['code'] = [self.uint() for _ in range(self.count())]
        return index

    def parse(self):
        if self.take(len(HEADER)) != HEADER:
            raise FormatError('unsupported header/version/VM layout')
        n = struct.unpack('<d', self.take(8))[0]
        if not (TEST_NUMBER - 1 < n < TEST_NUMBER + 1):
            raise FormatError('unsupported number format')
        self.function()
        return self.functions


def instruction(word):
    op, a, b, c = word & 63, word >> 24, (word >> 16) & 255, (word >> 6) & 1023
    if op >= len(OPS):
        raise FormatError(f'unknown opcode {op}')
    return OPS[op], a, b, c, (word >> 6) & 0x3ffff


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('file', type=Path)
    ap.add_argument('--offset', type=lambda x: int(x, 0), default=0)
    ap.add_argument('--function', type=int, help='print one preorder prototype')
    args = ap.parse_args()
    try:
        if args.file.stat().st_size > 128 * 1024 * 1024:
            raise FormatError('input exceeds 128 MiB')
        r = Reader(args.file.read_bytes(), args.offset)
        functions = r.parse()
        print(f'prototypes={len(functions)} end=0x{r.pos:x}')
        if args.function is not None:
            if not 0 <= args.function < len(functions):
                raise FormatError('prototype index outside chunk')
            f = functions[args.function]
            print(f"source={f['source']!r} line={f['line']} meta={f['meta']} children={f['children']}")
            for pc, word in enumerate(f['code'], 1):
                op, a, b, c, bc = instruction(word)
                note = ''
                if op in ('LOADK', 'GETGLOBAL', 'SETGLOBAL') and bc < len(f['constants']):
                    note = repr(f['constants'][bc])
                elif op in ('GETTABLE', 'SETTABLE', 'SELF', 'ADD', 'SUB', 'MUL', 'DIV', 'POW',
                            'TESTEQ', 'TESTNE', 'TESTLT', 'TESTLE', 'TESTGT', 'TESTGE') and 250 <= c < 250 + len(f['constants']):
                    note = repr(f['constants'][c - 250])
                elif op == 'CLOSURE' and bc < len(f['children']):
                    note = 'prototype ' + str(f['children'][bc])
                elif op == 'LOADINT':
                    note = str(bc - 131071)
                elif op in ('JMP', 'CJMP', 'FORLOOP', 'TFORLOOP'):
                    note = 'target ' + str(pc + 1 + bc - 131071)
                print(f'{pc:4} {op:12} A={a} B={b} C={c} Bc={bc} {note}')
    except (OSError, FormatError) as exc:
        ap.exit(2, f'error: {exc}\n')


if __name__ == '__main__':
    main()
