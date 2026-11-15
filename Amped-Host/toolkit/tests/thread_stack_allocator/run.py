"""Test actual stack allocation code against a controlled, thread-safe heap.

Usage: python run.py path/to/llvm-mingw/bin/clang.exe
The heap blocks two concurrent calls at allocation to expose the check/reserve
race. This isolates allocator accounting; it does not test the real heap or
thread creation/termination and does not modify the runtime.
"""
import pathlib
import re
import subprocess
import sys
import tempfile

here = pathlib.Path(__file__).resolve().parent
toolkit = here.parent.parent
source = (toolkit / 'src/kernel/xbox_memory_layout.c').read_text(encoding='utf-8')
parts = []
for pattern in (r'#define XBOX_THREAD_STACK_SIZE[^\n]*',
                r'#define XBOX_MAX_THREAD_STACKS[^\n]*',
                r'static volatile LONG g_thread_stacks_used[^\n]*'):
    match = re.search(pattern, source)
    if not match:
        raise RuntimeError(f'missing allocator declaration: {pattern}')
    parts.append(match.group())
for name in ('xbox_AllocThreadStack', 'xbox_FreeThreadStack'):
    match = re.search(r'^\w+[ \t]+' + name + r'\([^)]*\)\s*\{', source, re.M)
    if not match:
        raise RuntimeError(f'missing function: {name}')
    depth = 1
    for token in re.finditer(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]',
                             source[match.end():], re.S):
        if token.group() == '{': depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                parts.append(source[match.start():match.end() + token.end()])
                break
    else:
        raise RuntimeError(f'unterminated function: {name}')
test = (here / 'test_main.c').read_text(encoding='utf-8')
test = test.replace('/* ACTUAL_ALLOCATOR */', '\n'.join(parts))
with tempfile.TemporaryDirectory(prefix='thread-stack-test-') as tmp:
    tmp = pathlib.Path(tmp)
    (tmp / 'test.c').write_text(test, encoding='utf-8')
    exe = tmp / 'test.exe'
    subprocess.run([sys.argv[1], '-std=c11', '-O2', str(tmp / 'test.c'),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
