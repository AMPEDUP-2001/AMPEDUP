"""Exercise the Vulkan diagnostic's actual depth summary without a GPU.

Usage: python run.py path/to/llvm-mingw/bin/clang.exe
This does not test Vulkan copies, synchronization, or guest shadow semantics.
"""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

here = Path(__file__).resolve().parent
source = (here.parent.parent / 'src/d3d/nv2a_pb_vulkan.c').read_text(encoding='utf-8')
struct = re.search(r'struct amped_depth_stats\s*\{[^}]*\};', source, re.S)
start = re.search(r'static struct amped_depth_stats amped_depth_summarize\([^)]*\)\s*\{', source, re.S)
if not struct or not start:
    raise RuntimeError('depth summary definitions not found')
depth = 1
end = start.end()
for token in re.finditer(r'\{|\}', source[end:]):
    depth += 1 if token.group() == '{' else -1
    if not depth:
        end += token.end()
        break
else:
    raise RuntimeError('depth summary is unterminated')
test = (here / 'test_main.c').read_text(encoding='utf-8')
test = test.replace('/* ACTUAL_SUMMARY */', struct.group() + '\n' + source[start.start():end])
with tempfile.TemporaryDirectory(prefix='depth-probe-stats-') as tmp:
    src = Path(tmp) / 'test.c'
    exe = Path(tmp) / 'test.exe'
    src.write_text(test, encoding='utf-8')
    subprocess.run([sys.argv[1], '-std=c11', '-O2', str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
