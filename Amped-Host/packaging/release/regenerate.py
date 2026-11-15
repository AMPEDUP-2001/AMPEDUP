"""Regenerate the Windows game's C from the supported retail Xbox executable."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
from importlib.metadata import PackageNotFoundError, version
import json
from pathlib import Path
import subprocess
import sys


HOST = Path(__file__).resolve().parents[2]
INPUTS = Path(__file__).with_name("analysis")
WORK = HOST.parent / "Analysis"
XBE_SHA256 = "d825274c3a52b203e6065ef7b0109bcfffdc175fe8b826fac215a2adb779e58e"
BASE_SEEDS = (
    INPUTS / "observed-seeds-v23.json",
    INPUTS / "inferred-seeds-v11.json",
    INPUTS / "inferred-seeds-v12-extra.json",
)

CALLBACKS = (
    ("seeded-playeor-v23", "playeor/seed.json", "0002BE70", None),
    ("seeded-playeor-pair-v23", "playeor/seed-pair.json", "0002C0F0", None),
    ("seeded-menu-audit-v23", "playeor/seed-menu-audit.json", "000681A0",
     "replay_callback_v23/translated.txt"),
    ("seeded-multiplayer-v23", "multiplayer_callback_v23/seed.json", "00068960",
     "multiplayer_callback_v23/translated.txt"),
    ("seeded-career-56b40-v23", "career_56b40_v23/seed.json", "00056B40",
     "career_56b40_v23/translated.txt"),
    ("seeded-replay-save-4f880-v23", "replay_save_4f880_v23/seed.json", "0004F880",
     "replay_save_4f880_v23/translated.txt"),
)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run_tool(work, module, *args, output=None):
    log = work / (module + ".log")
    with log.open("w", encoding="utf-8") as stderr:
        if output is None:
            result = subprocess.run(
                [sys.executable, "-m", f"tools.{module}", *map(str, args)],
                cwd=HOST / "toolkit", stdout=stderr, stderr=subprocess.STDOUT,
            )
        else:
            with output.open("w", encoding="utf-8") as stdout:
                result = subprocess.run(
                    [sys.executable, "-m", f"tools.{module}", *map(str, args)],
                    cwd=HOST / "toolkit", stdout=stdout, stderr=stderr,
                )
    if result.returncode:
        raise RuntimeError(f"{module} exited with {result.returncode}. See {log}")


def analyze(xbe, work, analysis, extra_seed=None):
    print(f"Analyzing {work.name}", flush=True)
    work.mkdir(parents=True, exist_ok=False)
    seeds = [item for path in (*BASE_SEEDS, extra_seed) if path is not None
             for item in ("--seed-functions", path)]
    disasm = work / "disasm"
    funcs = work / "func_id"
    abi = work / "abi"
    run_tool(work, "disasm", xbe, "--analysis-json", analysis, *seeds, "-o", disasm)
    run_tool(work, "func_id", xbe, "--functions", disasm / "functions.json",
             "--strings", disasm / "strings.json", "--xrefs", disasm / "xrefs.json",
             "-o", funcs)
    run_tool(work, "abi_analysis", xbe, "--disasm-dir", disasm, "--func-id-dir",
             funcs, "--output-dir", abi)
    return ("--disasm-dir", disasm, "--func-id-dir", funcs, "--abi-dir", abi)


def callback(xbe, analysis, spec):
    name, seed, address, output = spec
    work = WORK / name
    tool_args = analyze(xbe, work, analysis, HOST / "probes" / seed)
    if output is None:
        name = "playeor-translation.txt" if address == "0002BE70" else "toggle-translation.txt"
        translation = work / name
    else:
        translation = HOST / "probes" / output
    run_tool(work, "recomp", xbe, "-f", address, *tool_args, output=translation)


def run_recipe(relative_path):
    subprocess.run([sys.executable, str(HOST / relative_path)], cwd=HOST, check=True)


def verify(root, key):
    index = json.loads((INPUTS / "generated-sha256.json").read_text(encoding="utf-8"))
    for relative, expected in index[key].items():
        path = root / relative
        if digest(path) != expected:
            raise RuntimeError(f"Generated source differs from the release recipe: {path}")
    print(f"Verified {len(index[key])} {key} files", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("xbe", type=Path, help="default.xbe extracted from the supported disc")
    parser.add_argument("--jobs", type=int, choices=range(1, 7), default=2,
                        help="parallel callback analyses")
    parser.add_argument("--verify-only", action="store_true",
                        help="validate an existing regeneration")
    args = parser.parse_args()
    xbe = args.xbe.resolve(strict=True)
    if digest(xbe) != XBE_SHA256:
        parser.error("default.xbe does not match the supported European retail executable.")
    base = WORK / "seeded-v23"
    if args.verify_only:
        verify(base / "generated", "baseline")
        verify(HOST / "probes", "patches")
        return
    try:
        capstone_version = version("capstone")
    except PackageNotFoundError:
        parser.error("Install capstone==5.0.9 into this Python environment.")
    if capstone_version != "5.0.9":
        parser.error("The release's regeneration uses capstone==5.0.9.")
    WORK.mkdir(exist_ok=False)
    analysis = WORK / "analysis.json"
    run_tool(WORK, "xbe_parser", xbe, "--json", analysis)
    tool_args = analyze(xbe, base, analysis)
    run_tool(base, "recomp", xbe, "--game-name", "Amped", "--all", "--split", "100",
             "--gen-dir", base / "generated", *tool_args,
             "--coalesce-functions", INPUTS / "amped-coalesce-40280.json", "-o", base / "recomp")
    verify(base / "generated", "baseline")
    with ThreadPoolExecutor(max_workers=args.jobs) as executor:
        for future in [executor.submit(callback, xbe, analysis, spec) for spec in CALLBACKS]:
            future.result()
    for recipe in ("prepare-boarder-v23-probe.py", "prepare-xinput-v23-probe.py",
                   "prepare-kick-wait-v23.py", "probes/audio_menu/prepare_fix.py",
                   "probes/playeor/prepare-candidate.py", "probes/replay_callback_v23/prepare.py",
                   "probes/multiplayer_callback_v23/prepare.py", "probes/career_56b40_v23/prepare.py",
                   "probes/replay_save_4f880_v23/prepare.py"):
        run_recipe(recipe)
    verify(HOST / "probes", "patches")


if __name__ == "__main__":
    main()
