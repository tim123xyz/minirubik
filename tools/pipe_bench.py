#!/usr/bin/env python3
"""Measure forwarding, branches and load-use hazards on Ripes RV32_5S."""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ripes", default=os.environ.get("RIPES"))
    parser.add_argument("--cc", default=os.environ.get(
        "RISCV_GCC", "/opt/riscv/bin/riscv32-unknown-elf-gcc"))
    args = parser.parse_args()
    output = Path(__file__).resolve().parent
    ripes = args.ripes or shutil.which("Ripes")
    if not ripes:
        candidates = sorted(Path("/tmp").glob("*/squashfs-root/usr/bin/Ripes"))
        if candidates:
            ripes = str(candidates[-1])
    if not ripes:
        parser.error("Set RIPES or --ripes to an extracted Ripes executable")

    loops = {
        "alu-forward": "add t3, t0, zero\nlhu t2, 0(t3)",
        "branch": "bnez zero, next\nnext:",
        "load-gap": "lhu t2, 0(t0)\naddi t3, t3, 1\nbeqz t2, next\nnext:",
        "load-use": "lhu t2, 0(t0)\nbeqz t2, next\nnext:",
    }
    results = []
    for name, loop in loops.items():
        source = output / f"pipe-{name}.S"
        elf = source.with_suffix(".elf")
        source.write_text(f""".option norelax
.section .rodata
.balign 4
value: .word 0
.section .text
.globl main
main:
la t0, value
li t1, 1000
loop:
{loop}
addi t1, t1, -1
bnez t1, loop
li a7, 10
ecall
""")
        subprocess.run([args.cc, "-march=rv32i", "-mabi=ilp32",
                        "-nostartfiles", "-e", "main", str(source),
                        "-o", str(elf)], check=True)
        run = subprocess.run([ripes, "--mode", "cli", "--src", str(elf),
                              "-t", "elf", "--proc", "RV32_5S",
                              "--timeout", "60000", "--json", "--cycles",
                              "--iret", "--runinfo"],
                             env={**os.environ, "QT_QPA_PLATFORM": "xcb"},
                             capture_output=True, text=True, timeout=70)
        raw = run.stdout + run.stderr
        (output / f"pipe-{name}.log").write_text(raw)
        run.check_returncode()
        telemetry, _ = json.JSONDecoder().raw_decode(
            run.stdout[run.stdout.index("{"):])
        result = {"benchmark": name, "iterations": 1000, **telemetry}
        results.append(result)
        print(json.dumps(result), flush=True)
    (output / "pipeline-results.json").write_text(
        json.dumps(results, indent=2) + "\n")


if __name__ == "__main__":
    main()
