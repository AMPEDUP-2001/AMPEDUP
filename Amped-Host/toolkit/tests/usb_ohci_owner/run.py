"""Build a small Windows test of the actual OHCI TD and physical-owner code.

Usage: python run.py path/to/llvm-mingw/bin/clang.exe
The source cut removes only OHCI's public entry points, which are unrelated
to TD execution and otherwise pull in the whole runtime at link time.
"""
import pathlib
import re
import subprocess
import sys
import tempfile

here = pathlib.Path(__file__).resolve().parent
toolkit = here.parent.parent
source = (toolkit / 'src/usb/ohci.c').read_text(encoding='utf-8')
names = ('xbox_OhciDirect', 'xbox_OhciHandleMmio', 'xbox_OhciInit',
         'xbox_OhciOwnsAddress', 'xbox_OhciReport')
for name in names:
    pattern = re.compile(r'^\w[\w\s*]*\b' + name + r'\s*\([^;]*?\)\s*\{', re.M | re.S)
    match = pattern.search(source)
    if not match:
        raise RuntimeError(f'OHCI entry point not found: {name}')
    start, i, depth = match.start(), match.end(), 1
    # These functions contain normal C strings and comments; count braces only
    # after removing their contents from the scan, without altering offsets.
    token = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*.*?\*/|//[^\n]*|[{}]', re.S)
    for t in token.finditer(source, i):
        if t.group() == '{': depth += 1
        elif t.group() == '}':
            depth -= 1
            if not depth:
                source = source[:start] + source[t.end():]
                break
    else:
        raise RuntimeError(f'unterminated function: {name}')
source = source.replace('"../platform/', '"platform/').replace('"../kernel/', '"kernel/')
test = (here / 'test_main.c').read_text(encoding='utf-8')
test = test.replace('#include "../../src/usb/ohci.c"', source)
with tempfile.TemporaryDirectory(prefix='ohci-owner-') as tmp:
    tmp = pathlib.Path(tmp)
    (tmp / 'test.c').write_text(test, encoding='utf-8')
    exe = tmp / 'test.exe'
    cmd = [sys.argv[1], '-std=c11', '-O2', '-ffunction-sections', '-fdata-sections']
    for sub in ('src', 'src/platform', 'src/kernel', 'src/usb'):
        cmd.extend(['-I', str(toolkit / sub)])
    cmd += [str(tmp / 'test.c'), str(toolkit / 'src/kernel/kernel_memory.c'),
            '-Wl,--gc-sections', '-o', str(exe)]
    subprocess.run(cmd, check=True)
    subprocess.run([str(exe)], check=True)
