"""Generate the small Menu.res patch recipe from a verified copy of the disc."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

HOST = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(HOST / 'research' / name) for name in ('lua41', 'vrtdrive')]
from inspect_chunk import OPS, Reader, instruction
from inspect_directory import directory


def u32(value):
    return struct.pack('<I', value)


def op(name, a=0, b=0, c=0):
    return OPS.index(name) | (a << 24) | (b << 16) | (c << 6)


def ab(name, a, value):
    return op(name, a, value >> 10, value & 1023)


def integer(register, value):
    return ab('LOADINT', register, 131071 + value)


class Prototype:
    def __init__(self, function, edits):
        self.fn = function
        self.edits = edits
        self.constants = function['constants']
        self.original_count = len(self.constants)
        self.words = function['code']
        self.offset = function['code_count_at'] + 4
        self.inserted = 0

    def const(self, value):
        if value not in self.constants:
            self.constants.append(value)
        return self.constants.index(value)

    def key(self, value):
        return self.const(value) + 250

    def load(self, reg, value):
        return ab('LOADK', reg, self.const(value))

    def global_(self, reg, name):
        return ab('GETGLOBAL', reg, self.const(name))

    def set(self, table, field, value):
        reg = table + 1
        load = integer(reg, value) if isinstance(value, int) else self.load(reg, value)
        return [load, op('SETTABLE', reg, table, self.key(field))]

    def replace_load(self, old, new, reg=3):
        word = self.load(reg, old)
        matches = [i for i, value in enumerate(self.words) if value == word]
        if len(matches) != 1:
            raise ValueError(f'Expected one load of {old}')
        self.edits.append((self.offset + 4 * matches[0], 4, u32(self.load(reg, new))))

    def insert(self, position, words):
        data = b''.join(map(u32, words))
        self.edits.append((self.offset + 4 * position, 0, data))
        self.inserted += len(words)

    def before_return(self, words):
        if instruction(self.words[-1])[0] != 'RETURN':
            raise ValueError('Unexpected prototype ending')
        self.insert(len(self.words) - 1, words)

    def notify(self, command):
        return [self.global_(1, 'LaunchXboxDemos'), integer(2, command), op('CALL', 1, 1, 0)]

    def menu_model(self, menu, control):
        # CacheMenu attaches model wrappers to the already-declared controls.
        # The wrapper can still be nil on activation while the panel loads.
        return [self.global_(1, 'Menus'), op('GETTABLE', 1, 1, self.key(menu)),
                op('GETTABLE', 1, 1, self.key('Controls')),
                op('GETTABLE', 1, 1, self.key(control)),
                op('GETTABLE', 1, 1, self.key('model'))]

    def show_help(self, menu, show):
        return self.menu_model(menu, 'help_text') + [
                op('TESTF', 1, 1), ab('CJMP', 0, 131073),
                op('SELF', 2, 1, self.key('Show' if show else 'Hide')), op('CALL', 2, 1)]

    def finish(self, stack=None):
        if self.inserted:
            self.edits.append((self.offset - 4, 4, u32(len(self.words) + self.inserted)))
        added = self.constants[self.original_count:]
        if added:
            encoded = bytearray()
            for value in added:
                if isinstance(value, str):
                    raw = value.encode('ascii') + b'\0'
                    encoded += b'\3' + u32(len(raw)) + raw
                else:
                    encoded += b'\2' + struct.pack('<d', value)
            self.edits.extend([
                (self.fn['constant_count_at'], 4, u32(len(self.constants))),
                (self.fn['constant_end'], 0, bytes(encoded)),
            ])
        if stack is not None and stack > self.fn['meta'][3]:
            stack_at = self.fn['stack_size_at']
            self.edits.append((stack_at, 4, u32(stack)))


def insert_control(menu, last, functions, edits, native=False):
    extra = last + 1
    control = f'_Control{extra}'
    from_first, from_last = f'_Button1to{extra}', f'_Button{last}to{extra}'
    to_first, to_last = f'_Button{extra}to1', f'_Button{extra}to{last}'
    actions = (from_first, from_last, to_first, to_last)
    top = Prototype(functions[0], edits)
    top.replace_load(f'_Button1to{last}', from_first)
    top.replace_load(f'_Button{last}to1', from_last)
    words = [top.global_(0, 'Menus'), op('GETTABLE', 0, 0, top.key(menu)),
             op('GETTABLE', 1, 0, top.key('Controls')), ab('NEWTABLE', 2, 8)]
    for field, value in (('Up', to_last), ('Down', to_first), ('AButton', 103),
                         ('Start', 103), ('Back', 2 if last == 6 else 1),
                         ('BButton', 2 if last == 6 else 1)):
        words += top.set(2, field, value)
    if last == 5:
        words += top.set(2, 'XButton', 2)
    help_key = f'{menu}Button{extra}Help' if native else ''
    if native:
        words += top.set(2, 'HelpText', help_key)
    words += [op('SETTABLE', 2, 1, top.key(control)), op('GETTABLE', 1, 0, top.key('Actions'))]
    for action in actions:
        words += [ab('NEWTABLE', 2, 1), *top.set(2, 'action', 'Empty'),
                  op('SETTABLE', 2, 1, top.key(action))]
    top.before_return(words)
    top.finish()

    dispatch = Prototype(functions[1], edits)
    select = [dispatch.global_(1, 'SetActiveControl'), dispatch.load(2, control),
              dispatch.load(3, help_key), op('CALL', 1, 2)]
    select += dispatch.show_help(menu, native)
    words = []
    for value, body in (
        (101, select + dispatch.notify(3)),
        (102, dispatch.show_help(menu, True) + dispatch.notify(4)),
        (103, dispatch.notify(5 if last == 6 else 6)),
    ):
        words.extend([op('TESTNE', 0, 0, dispatch.key(float(value))),
                      ab('CJMP', 0, 131071 + len(body) + 1), *body, op('RETURN')])
    dispatch.insert(0, words)
    dispatch.finish()

    notify_title_lifecycle(menu, functions, edits, 1 if last == 6 else 2, native)
    return actions


def notify_title_lifecycle(menu, functions, edits, command, native):
    for index, event in ((-3, command), (-2, 7)):
        lifecycle = Prototype(functions[index], edits)
        words = lifecycle.notify(event + 9 if native and event in (1, 2) else event)
        if event != 7:
            words += lifecycle.show_help(menu, True)
        lifecycle.insert(0, words)
        lifecycle.finish(stack=4)


def replace_demo_control(functions, edits):
    # Keep the original sixth control, its neighbors, and its animation.
    # Dispatch A and Start directly so the disc's demo transition never runs.
    top = Prototype(functions[0], edits)
    old = top.load(3, 'ToLaunchDemos')
    control = op('SETTABLE', 2, 1, top.key('_Control6'))
    end, = [i for i, word in enumerate(top.words) if word == control]
    start = max(i for i in range(end) if instruction(top.words[i])[:2] == ('NEWTABLE', 2))
    for field in ('AButton', 'Start'):
        assignment = op('SETTABLE', 3, 2, top.key(field))
        match, = [i for i in range(start + 1, end)
                  if top.words[i:i + 2] == [old, assignment]]
        edits.append((top.offset + match * 4, 4, u32(integer(3, 103))))
    top.replace_load('GameDemos', 'Career', reg=2)

    dispatch = Prototype(functions[1], edits)
    dispatch.replace_load('GameDemos', 'Career', reg=2)
    body = dispatch.notify(5)
    dispatch.insert(0, [op('TESTNE', 0, 0, dispatch.key(103.0)),
                        ab('CJMP', 0, 131072 + len(body)), *body, op('RETURN')])
    dispatch.finish()
    notify_title_lifecycle('MainMenu', functions, edits, 1, native=True)


def insert_actions(function, actions, last, edits, native=False):
    proto = Prototype(function, edits)
    stack = None
    sound = 8 if last == 6 else 4
    word_groups = (
        ((f'@_Button1Normal', sound), (101,)),
        ((f'@_Button{last}Normal', sound), (101,)),
        ((102,), ('@_Button1ActiveDown',)),
        ((102,), (f'@_Button{last}ActiveUp',)),
    )
    if native:
        extra = f'button{last + 1}'
        on = (extra + 'selected', extra + 'bounce')
        off = (extra + 'unselected',)
        word_groups = tuple(steps + (on,) if index < 2 else (off,) + steps
                            for index, steps in enumerate(word_groups))
        # The scene can retain its animated materials when Options is reopened.
        # Run the Graphics deselection as its own step before showing the panel.
        stack = max(7, *(prepend_animation(proto, action, off[0])
                         for action in ('MenuIn', 'SwapToOptionsMenu')))
    words = []
    for action, steps in zip(actions, word_groups, strict=True):
        words.append(ab('NEWTABLE', 1, len(steps)))
        for i, step in enumerate(steps, start=2):
            words.append(ab('NEWTABLE', i, len(step)))
            for j, arg in enumerate(step, start=i + 1):
                words.append(proto.load(j, arg) if isinstance(arg, str) else integer(j, arg))
            words.append(op('SETLIST', i, 0, len(step) - 1))
        words.extend([op('SETLIST', 1, 0, len(steps) - 1), op('SETTABLE', 1, 0, proto.key(action))])
    if instruction(proto.words[-2])[:2] != ('SETGLOBAL', 0):
        raise ValueError('Missing Actions assignment')
    proto.insert(len(proto.words) - 2, words)
    proto.finish(stack=stack)


def prepend_animation(proto, action, animation):
    end, = [i for i, word in enumerate(proto.words) if word == op('SETTABLE', 1, 0, proto.key(action))]
    start = max(i for i in range(end) if proto.words[i] == ab('NEWTABLE', 1, 2))
    if proto.words[end - 1] != op('SETLIST', 1, 0, 1):
        raise ValueError(f'Unexpected action list for {action}')
    original = proto.words[start + 1:end - 1]
    for word in original:
        opcode, reg, *_ = instruction(word)
        if reg < 2 or opcode not in ('NEWTABLE', 'LOADK', 'LOADINT', 'SETLIST'):
            raise ValueError(f'Unexpected action instruction for {action}')
    shifted = [word + (1 << 24) for word in original]
    words = [ab('NEWTABLE', 1, 3), ab('NEWTABLE', 2, 1), proto.load(3, animation),
             op('SETLIST', 2, 0, 0), *shifted, op('SETLIST', 1, 0, 2)]
    proto.edits.append((proto.offset + start * 4, (end - start) * 4, b''.join(map(u32, words))))
    proto.inserted += len(words) - (end - start)
    return max(instruction(word)[1] + 1 for word in words)


def add_music_action(functions, edits):
    top = Prototype(functions[0], edits)
    listbox = op('SETTABLE', 2, 1, top.key('listbox'))
    if top.words.count(listbox) != 1:
        raise ValueError('Expected one Soundtracks listbox')
    top.insert(top.words.index(listbox), top.set(2, 'YButton', 12))
    top.finish()

    dispatch = Prototype(functions[1], edits)
    words = []
    for action, command in ((6, 8), (7, 7), (12, 9)):
        body = dispatch.notify(command)
        words.extend([op('TESTNE', 0, 0, dispatch.key(float(action))),
                      ab('CJMP', 0, 131071 + len(body)), *body])
    dispatch.insert(0, words)
    dispatch.finish()


def menu_edits(data, replacements=None, native=False):
    entries = {name: (offset, length) for name, offset, length in directory(data)}
    archive_edits = []
    deltas = {}
    for name, payload in (replacements or {}).items():
        offset, length = entries[name]
        archive_edits.append((offset, length, payload))
        deltas[name] = len(payload) - length
    for basename, menu, last in (('mainmenu', 'MainMenu', 6), ('optionsmenu', 'OptionsMenu', 5)):
        for suffix in ('.blua', '.actions.blua'):
            if native and basename == 'mainmenu' and suffix == '.actions.blua':
                continue
            name = basename + suffix
            offset, length = entries[name]
            reader = Reader(data, offset)
            functions = reader.parse()
            if reader.pos != offset + length:
                raise ValueError(f'Unexpected Lua chunk length for {name}')
            edits = []
            if native and basename == 'mainmenu':
                replace_demo_control(functions, edits)
            elif suffix == '.blua':
                actions = insert_control(menu, last, functions, edits, native=native)
            else:
                insert_actions(functions[0], actions, last, edits, native=native)
            deltas[name] = sum(len(payload) - size for _, size, payload in edits)
            archive_edits.extend(edits)

    name = 'soundtracksmenu.blua'
    offset, length = entries[name]
    reader = Reader(data, offset)
    functions = reader.parse()
    if reader.pos != offset + length:
        raise ValueError(f'Unexpected Lua chunk length for {name}')
    edits = []
    add_music_action(functions, edits)
    deltas[name] = sum(len(payload) - size for _, size, payload in edits)
    archive_edits.extend(edits)

    return sorted(archive_edits + directory_edits(data, entries, deltas))


def directory_edits(data, entries, deltas):
    directory_at = struct.unpack_from('<I', data, 12)[0]
    changes = [(12, 4, u32(directory_at + sum(deltas.values())))]
    position = directory_at + 8
    for name, (offset, length) in entries.items():
        position += 1 + len(name) + 4
        shift = sum(delta for key, delta in deltas.items() if entries[key][0] < offset)
        if shift or name in deltas:
            changes.append((position, 8, struct.pack('<II', offset + shift, length + deltas.get(name, 0))))
        position += 28
    return changes


def patched(source, edits):
    chunks = []
    end = 0
    for offset, length, payload in edits:
        if offset < end or offset + length > len(source):
            raise ValueError('Overlapping or out-of-bounds menu edits')
        chunks.extend([source[end:offset], payload])
        end = offset + length
    return b''.join([*chunks, source[end:]])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('recipe', type=Path)
    args = parser.parse_args()
    recipe = json.loads(args.recipe.read_text(encoding='utf-8'))
    source = args.source.read_bytes()
    if len(source) != recipe['size'] or hashlib.sha256(source).hexdigest() != recipe['source_sha256']:
        raise ValueError('Unsupported original menu resource')
    edits = menu_edits(source)
    recipe['menu_edits'] = [[offset, length, payload.hex()] for offset, length, payload in edits]
    for mode in recipe['modes'].values():
        data = bytearray(source)
        for index, before, after in mode['edits']:
            if data[index] != before:
                raise ValueError('Existing resource patch mismatch')
            data[index] = after
        result = patched(data, edits)
        for name, offset, length in directory(result):
            if name in ('mainmenu.blua', 'mainmenu.actions.blua', 'optionsmenu.blua',
                        'optionsmenu.actions.blua', 'soundtracksmenu.blua'):
                reader = Reader(result, offset)
                reader.parse()
                if reader.pos != offset + length:
                    raise ValueError(f'Patched Lua chunk length mismatch for {name}')
        mode['sha256'] = hashlib.sha256(result).hexdigest()
    args.recipe.write_text(json.dumps(recipe, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
