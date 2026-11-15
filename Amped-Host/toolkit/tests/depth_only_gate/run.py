"""Exercise the actual opt-in Vulkan depth-only selection predicate.

Usage: python run.py path/to/llvm-mingw/bin/clang.exe
This tests selection only; it does not exercise Vulkan, image lifetime or depth writes.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
src = (here.parent.parent / 'src/d3d/nv2a_pb_vulkan.c').read_text(encoding='utf-8')
match = re.search(r'static int depth_only_wanted\([^}]+\}', src, re.S)
if not match:
    raise RuntimeError('depth-only predicate not found')
test = (here / 'test_main.c').read_text(encoding='utf-8')
test = test.replace('/* ACTUAL_PREDICATE */', match.group())
with tempfile.TemporaryDirectory(prefix='depth-only-gate-') as tmp:
    c = Path(tmp) / 'test.c'
    exe = Path(tmp) / 'test.exe'
    c.write_text(test, encoding='utf-8')
    subprocess.run([sys.argv[1], '-std=c11', '-O2', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
