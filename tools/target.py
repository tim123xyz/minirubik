#!/usr/bin/env python3
"""Check min.S and ida.S on Ripes without changing the original sources."""
import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"
MOVES = ("R", "R2", "R'", "B", "B2", "B'", "D", "D2", "D'")
SOURCES = ((1, 4, 2, 0, 3, 5, 6), (0, 1, 2, 4, 5, 6, 3),
           (0, 2, 5, 3, 1, 4, 6))
TWISTS = ((1, 2, 0, 2, 1, 0, 0), (0, 0, 0, 1, 2, 1, 2), (0,) * 7)
DEFAULT_CASES = ("12345671111111", "62345713133111",
                 "21345671111111", "54721631111111")


def valid_state(code):
    return (len(code) == 14 and sorted(code[:7]) == list("1234567")
            and all(c in "123" for c in code[7:])
            and sum(int(c) - 1 for c in code[7:]) % 3 == 0)


def solves(code, moves):
    p = [int(c) - 1 for c in code[:7]]
    o = [int(c) - 1 for c in code[7:]]
    for move in moves:
        if move not in MOVES:
            return False
        face = "RBD".index(move[0])
        turns = 2 if move.endswith("2") else 3 if move.endswith("'") else 1
        for _ in range(turns):
            p = [p[i] for i in SOURCES[face]]
            o = [(o[i] + TWISTS[face][j]) % 3
                 for j, i in enumerate(SOURCES[face])]
    return p == list(range(7)) and o == [0] * 7


def executable(path):
    return pathlib.Path(path).is_file() and os.access(path, os.X_OK)


def find_ripes():
    configured = os.environ.get("RIPES")
    if configured:
        resolved = shutil.which(configured) or configured
        if not executable(resolved):
            raise RuntimeError(f"RIPES is not executable: {configured}")
        return str(pathlib.Path(resolved).resolve())
    for name in ("Ripes", "ripes"):
        resolved = shutil.which(name)
        if resolved:
            return resolved
    candidates = sorted(pathlib.Path("/tmp").glob("*/squashfs-root/usr/bin/Ripes"))
    candidates += sorted(pathlib.Path("/tmp").glob("appimage_extracted*/usr/bin/Ripes"))
    for path in candidates:
        if executable(path):
            return str(path)
    raise RuntimeError("Ripes was not found; set RIPES to the extracted executable")


def compiler():
    configured = os.environ.get("RISCV_GCC")
    resolved = (shutil.which(configured) or configured) if configured else (
        shutil.which("riscv32-unknown-elf-gcc") or
        "/opt/riscv/bin/riscv32-unknown-elf-gcc")
    if not executable(resolved):
        raise RuntimeError(f"RISC-V compiler not found: {resolved}; set RISCV_GCC")
    return str(resolved)


def build(solver, code, render, cc):
    source = (ROOT / f"{solver}.S").read_text()
    source, count = re.subn(r'(\binput:\s*\.string\s*)"[^"]*"',
                            lambda m: m[1] + f'"{code}"', source)
    if count != 1:
        raise RuntimeError(f"{solver}.S: expected one labeled input string")
    if solver == "ida":
        source, count = re.subn(r"(?m)^\.equ\s+RENDER,\s*[01]\s*$",
                               f".equ RENDER, {int(render)}", source)
        if count != 1:
            raise RuntimeError("ida.S: expected one RENDER setting")
    tag = f"{solver}{'-render' if solver == 'ida' and render else ''}-{code}"
    asm = TOOLS / f"target-{tag}.S"
    elf = asm.with_suffix(".elf")
    asm.write_text(source)
    inputs = [str(asm)] + ([str(ROOT / "table.S")] if solver == "ida" else [])
    subprocess.run([cc, "-march=rv32i", "-mabi=ilp32", "-nostartfiles",
                    "-e", "main", *inputs, "-o", str(elf)], check=True)
    size_tool = pathlib.Path(cc).with_name(pathlib.Path(cc).name.replace("gcc", "size"))
    sizes = {}
    if executable(size_tool):
        output = subprocess.check_output([str(size_tool), "-A", str(elf)], text=True)
        sections = dict((name, int(size)) for name, size in
                        re.findall(r"(?m)^(\.\S+)\s+(\d+)\s+\d+\s*$", output))
        sizes = {name[1:] + "_bytes": sections.get(name, 0)
                 for name in (".text", ".rodata", ".data", ".bss")}
    return elf, tag, sizes


def parse_output(output, model, elf):
    exit_line = re.search(r"Program exited with code:\s*(-?\d+)\s*\n", output)
    if not exit_line or int(exit_line[1]) != 0:
        raise ValueError("no successful simulated program exit")
    moves = output[:exit_line.start()].replace("\0", "").split()
    if any(move not in MOVES for move in moves):
        raise ValueError("unexpected tokens in program output")
    telemetry = json.loads(output[exit_line.end():])
    for key in ("# instructions retired", "cycles", "execution time (ms)"):
        value = telemetry[key]
        if type(value) is not int or value < 0:
            raise ValueError(f"invalid telemetry field: {key}")
    if not telemetry["# instructions retired"] or not telemetry["cycles"]:
        raise ValueError("simulation did not execute any instructions")
    info = telemetry["runinfo"]
    if info["processor"] != model or pathlib.Path(info["source file"]).resolve() != elf:
        raise ValueError("telemetry does not identify the requested model and ELF")
    return moves, telemetry


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("states", nargs="*", help="legal 14-digit states")
    parser.add_argument("--solver", choices=("min", "ida", "all"), default="all")
    parser.add_argument("--render", action="store_true",
                        help="keep ida.S LED drawing and frame delays enabled")
    parser.add_argument("--timeout", type=int, default=600000,
                        help="per-model simulation timeout in milliseconds")
    args = parser.parse_args()
    cases = args.states or DEFAULT_CASES
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    for code in cases:
        if not valid_state(code):
            parser.error(f"invalid state: {code}")
    ripes, cc = find_ripes(), compiler()
    oracle = TOOLS / "oracle"
    if not executable(oracle):
        raise RuntimeError("tools/oracle is missing; run tools/run.sh or build tools/oracle.c")
    expected = {}
    for code in cases:
        moves = subprocess.check_output([str(oracle), code], text=True).split()
        if not solves(code, moves):
            raise RuntimeError(f"oracle returned an invalid path for {code}")
        expected[code] = len(moves)
    solvers = ("min", "ida") if args.solver == "all" else (args.solver,)
    for solver in solvers:
        for code in cases:
            elf, tag, sizes = build(solver, code, args.render, cc)
            for model in ("RV32_ISS", "RV32_5S"):
                started = time.monotonic()
                result = subprocess.run(
                    [ripes, "--mode", "cli", "--src", str(elf), "-t", "elf",
                     "--proc", model, "--timeout", str(args.timeout), "--json",
                     "--cycles", "--iret", "--exectime", "--runinfo"],
                    env={**os.environ, "QT_QPA_PLATFORM": "xcb"},
                    capture_output=True, text=True, timeout=args.timeout / 1000 + 30)
                log = TOOLS / f"{tag}-{model}.log"
                log.write_text(result.stdout + ("\n[stderr]\n" + result.stderr
                                                if result.stderr else ""))
                report = dict(solver=solver, state=code, model=model,
                              render=solver == "ida" and args.render,
                              pass_gate=False, exact_length=expected[code],
                              wall_seconds=time.monotonic() - started,
                              log=str(log.relative_to(ROOT)), **sizes)
                try:
                    moves, telemetry = parse_output(result.stdout, model, elf)
                    report.update(moves=moves, instructions=telemetry["# instructions retired"],
                                  cycles=telemetry["cycles"],
                                  execution_ms=telemetry["execution time (ms)"])
                    report["pass_gate"] = (result.returncode == 0 and solves(code, moves)
                                           and len(moves) == expected[code])
                    if not report["pass_gate"]:
                        report["error"] = "exit status, solved state, or optimal length mismatch"
                except (ValueError, KeyError, TypeError) as error:
                    report["error"] = str(error)
                print(json.dumps(report), flush=True)
                if not report["pass_gate"]:
                    return 1
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (RuntimeError, OSError, subprocess.SubprocessError) as error:
        print(f"target.py: {error}", file=sys.stderr)
        sys.exit(1)
