# Correctness and Ripes measurements

Run from the repository root. All binaries, input-specific assembly copies and
logs are written under `tools/`; the existing solver sources are read unchanged.

```sh
tools/run.sh
```

This builds the host harnesses and the `solver.c` oracle, checks `min.cpp` on
all 3,674,160 states, checks IDA tables/heuristic globally and 100,000 IDA paths,
then runs four inputs for both assembly solvers on `RV32_ISS` and `RV32_5S`.
The min exhaustive search previously took about 10 minutes. To check all IDA
paths as well, set `IDA_FULL=1`; the default IDA path sample reports PARTIAL.

```sh
IDA_FULL=1 tools/run.sh
HOST_LIMIT=24 IDA_SAMPLE=300 IDA_LOG=tools/ida-smoke.log TARGET_LOG=tools/target-smoke.log tools/run.sh --solver ida 12345671111111
```

The second command is a quick integration check; its min and IDA H3 results
are explicitly PARTIAL. It does not replace an exhaustive run.

## Host checks

`host.cpp` includes the actual `min.cpp` search and the `solver.c` model in
separate namespaces, captures the returned moves, and checks exact lengths and
replays. H1 is inapplicable to bidirectional BFS. H2 checks every quarter-turn
transition; its runtime visited-state table is intentionally partial.
H4 checks byte accessors at every index, with no packed accessor.

The current min consteval builders call non-constexpr functions, so the raw
source cannot compile directly. Scoped keyword macros in the host harness and
`min_main.cpp` make only those tables const runtime initializers; all standard
headers load before the macros, and the macros are immediately removed after
inclusion. Table values and search bodies stay unchanged, and H2 checks their
values against the oracle. This adaptation is confined to test executables.

`ida_host.cpp` includes the actual `ida.cpp` through `IDA_NO_MAIN`. It builds
an independent full BFS using only `solver.c` moves, verifies all candidate
transitions and the exact depth-5 records, checks every heuristic bound and
rank round trip, replays every stored suffix, and compares `table.S` data and
all 257 prefix boundaries. IDA H3 executes `ida_star` and replays with the
oracle. Sampling selects distinct deterministic states and covers depths 0–11.

```sh
./tools/host                         # exhaustive min H3
./tools/host 24                      # partial min H3
./tools/ida-host --sample 100000     # global H1/H2/H4, sampled H3
./tools/ida-host --full              # exhaustive IDA H3
```

IDA uses exact distance for states within depth 5, and lower bound 6 elsewhere.
Its sparse table is complete for that declared domain. H4 verifies the
32-bit key/depth/move fields; there is no packed-nibble distance accessor.
Host checks establish diameter 11; target samples alone do not.

`oracle.c` suppresses legacy `free()` calls on `solver.c`'s static buffers
only in its test translation unit. The host harnesses call the model directly
without invoking those cleanup paths.

## Target checks and LED builds

```sh
python3 tools/target.py
python3 tools/target.py --solver min 21345671111111
python3 tools/target.py --solver ida 54721631111111
python3 tools/target.py --solver ida --render 62345713133111
```

The default is `--solver all`, testing solved, depth 8, required depth 11,
and IDA's embedded depth-11 input. Any additional legal 14-digit states work.
The tool changes the labeled input in a copy, links IDA with existing
`table.S`, independently replays each output, and compares its length with
the native oracle. It requires successful simulated exit and matching JSON
telemetry. Different shortest paths are accepted.

IDA defaults to `RENDER=0` in the generated copy for search measurements.
`--render` keeps drawing and frame delays enabled; open the generated ELF in
Ripes GUI with an 8×6 LED matrix at `0xf0000000`. CLI path verification with
drawing enabled does not verify the GUI's pixels. The original `ida.S`
remains `RENDER=1`. Both assembly parsers assume legal inputs.

Set `RIPES` to the extracted Ripes executable and `RISCV_GCC` to the RV32I
compiler if needed. Ripes is otherwise discovered on PATH or in extracted
AppImages under `/tmp`; the compiler falls back to `/opt/riscv/bin/`.
Qt uses the available `xcb` platform.

## Pipeline checks and logs

Search effort can be measured over every state independently of target timing:

```sh
python3 tools/profile_ida.py
```

The generator adds only a counter declaration and a counter increment to a
copy of `ida.cpp` under `tools/`. A node is the first entry into a search
frame, including the root, immediate threshold pruning, table hits and
repeated visits across thresholds; appending the table suffix adds no nodes.
The harness verifies every solution against an independently generated
`solver.c` BFS and writes per-distance and global mean/worst counts to
`search-stats.json`, with progress and timings in `ida-profile.log`.

```sh
python3 tools/pipe_bench.py
```

Four 1,000-iteration RV32_5S loops isolate ALU forwarding, not-taken branch,
load-use dependency and an independent instruction between load and use.
Raw telemetry is preserved; `pipeline-results.json` contains the summary.

| Artifact | Scope |
| --- | --- |
| `host.log` | Existing/full min host gates; retains the 2026-10-03 result until a full rerun |
| `host-partial.log` | Explicitly limited min run |
| `ida-host.log` | Global IDA table gates plus sampled H3 |
| `ida-full.log` | Exhaustive IDA H3 |
| `target-current.log` | Most recent target summary (JSON Lines) |
| `min-STATE-MODEL.log`, `ida-STATE-MODEL.log` | Per-solver raw target output and telemetry |
| `ida-render-STATE-MODEL.log` | Drawing-enabled target output |
| `target-render.log` | Recorded drawing-enabled summary |
| `target-SOLVER-STATE.elf`, `target-ida-render-STATE.elf` | Generated ELF for CLI/GUI |
| `pipe-NAME.log`, `pipeline-results.json` | Pipeline measurements |

The older unprefixed per-state logs and `target.log` are preserved as historical
min evidence. Section sizes in target summaries include static ELF data;
min's runtime table/queue allocation is additional. See [RESULTS.md](RESULTS.md)
for dates, measured figures and verification limits.
