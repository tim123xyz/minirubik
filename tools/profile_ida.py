#!/usr/bin/env python3
"""Profile the existing ida.cpp on the complete independent solver.c graph."""
from pathlib import Path
import hashlib
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
TOOLS = ROOT / "tools"


def main():
    source = (ROOT / "ida.cpp").read_text()
    function = "static bool ida_star(rank_t target, uint8_t *length)\n{"
    entry = "            if (frame.next_move == 0) {\n"
    declaration = "static uint64_t profile_nodes = 0;\n\n"
    increment = "                ++profile_nodes;\n"
    assert source.count(function) == source.count(entry) == 1
    instrumented = source.replace(function, declaration + function)
    instrumented = instrumented.replace(entry, entry + increment)
    assert instrumented.replace(declaration, "", 1).replace(increment, "", 1) == source
    (TOOLS / "ida_profile_impl.inc").write_text(instrumented)
    host = (TOOLS / "ida_host.cpp").read_text()
    prefix, _ = host.split("int main(int argc, char **argv)\n", 1)
    assert prefix.count('#include "../ida.cpp"') == 1
    (TOOLS / "ida_profile_oracle.inc").write_text(
        prefix.replace('#include "../ida.cpp"', '#include "ida_profile_impl.inc"')
    )
    digest = hashlib.sha256(source.encode()).hexdigest()
    subprocess.run([
        "g++", "-O3", "-std=c++20", "-fconstexpr-ops-limit=1000000000",
        str(TOOLS / "ida_profile.cpp"), "-o", str(TOOLS / "ida-profile"),
    ], cwd=ROOT, check=True)
    command = [str(TOOLS / "ida-profile"), digest]
    if len(sys.argv) > 1:
        command.extend(sys.argv[1:])
    with (TOOLS / "ida-profile.log").open("w") as log:
        print(f"Generated instrumentation PASS: only uint64_t counter declaration and frame-entry increment; ida.cpp SHA-256={digest}", file=log, flush=True)
        process = subprocess.Popen(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1)
        for line in process.stdout:
            print(line, end="", flush=True)
            log.write(line)
            log.flush()
        if process.wait():
            raise SystemExit(process.returncode)


if __name__ == "__main__":
    main()
