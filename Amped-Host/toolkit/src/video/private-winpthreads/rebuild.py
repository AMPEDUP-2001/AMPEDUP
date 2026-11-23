"""Reproduce the packaged archive in a fresh directory with a supplied LLVM-MinGW 20260922 x64 SDK."""
import argparse
from hashlib import sha256
import json
from pathlib import Path
import re
import shutil
import subprocess

HERE=Path(__file__).resolve().parent
MEMBER="libwinpthread_la-thread.o"


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def build(toolchain,out):
    meta=json.loads((HERE/"provenance.json").read_bytes())
    for rel,expected in meta["files"].items():
        if digest(HERE/rel)!=expected:
            raise ValueError("Dependency source or reference hash mismatch: "+rel)
    cc,ar=toolchain/"bin/clang.exe",toolchain/"bin/llvm-ar.exe"
    original=toolchain/"x86_64-w64-mingw32/lib/libwinpthread.a"
    if digest(original)!=meta["original_archive_sha256"]:
        raise ValueError("Unexpected toolchain archive")
    out.mkdir(parents=True,exist_ok=False)
    src=out/"thread.c"
    shutil.copyfile(HERE/"patched/thread.c",src)
    # The pinned reference's optional DWARF records the original SDK header prefix.
    # Carry that existing spelling into an exact rebuild when the SDK is relocated.
    ref=subprocess.check_output([ar,"p",HERE/"libwinpthread.a",MEMBER])
    prefixes=set(re.findall(rb"([A-Za-z]:[\x20-\x7e]*?/llvm-mingw)/include\x00",ref))
    if len(prefixes)!=1:
        raise ValueError("Expected one SDK prefix in the archived reference")
    prefix=prefixes.pop().decode("ascii")
    obj=out/MEMBER
    flags=["--target=x86_64-w64-mingw32","-g","-O2","-mguard=cf","-DIN_WINPTHREAD",
           "-D__USE_MINGW_ANSI_STDIO=0","-fdebug-compilation-dir=/winpthreads",
           "-ffile-prefix-map="+HERE.as_posix()+"=/winpthreads"]
    if toolchain.as_posix()!=prefix:
        flags.append("-ffile-prefix-map="+toolchain.as_posix()+"="+prefix)
    subprocess.run([cc,*flags,"-I",HERE/"original","-c","thread.c","-o",obj.name],cwd=out,check=True)
    data=bytearray(obj.read_bytes()); data[4:8]=bytes(4); obj.write_bytes(data)
    private=out/"libwinpthread.a"
    shutil.copyfile(original,private)
    subprocess.run([ar,"rD",private,obj],check=True)
    if digest(private)!=meta["artifact_sha256"]:
        raise ValueError("Rebuilt archive differs from pinned reference")
    print(digest(private))


if __name__=="__main__":
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--toolchain",type=Path,required=True)
    ap.add_argument("--out",type=Path,required=True)
    ns=ap.parse_args()
    build(ns.toolchain.resolve(strict=True),ns.out.resolve())
