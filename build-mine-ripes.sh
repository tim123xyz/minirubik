sed '/^int solver_main/,$d' ripes_support.c |
/opt/riscv/bin/riscv32-unknown-elf-gcc \
  -O2 -march=rv32i -mabi=ilp32 -mno-relax \
  -x c -c -o /tmp/mine-ripes-support.o -

/opt/riscv/bin/riscv32-unknown-elf-g++ \
  -O2 -std=gnu++20 -march=rv32i -mabi=ilp32 -mno-relax \
  mine.cpp /tmp/mine-ripes-support.o \
  -o mine-ripes.elf