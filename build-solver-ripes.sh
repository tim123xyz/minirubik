#!/bin/sh
set -eu
CC=${RISCV_CC:-/opt/riscv/bin/riscv32-unknown-elf-gcc}
OBJ=$(mktemp)
trap 'rm -f "$OBJ"' EXIT HUP INT TERM
"$CC" -O2 -std=c99 -march=rv32i -mabi=ilp32 -mno-relax -Dmain=solver_main -c solver.c -o "$OBJ"
"$CC" -O2 -std=c99 -march=rv32i -mabi=ilp32 -mno-relax "$OBJ" ripes_support.c -o solver-ripes.elf