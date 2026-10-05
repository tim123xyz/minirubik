#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
g++ -O3 -std=c++20 tools/host.cpp -o tools/host
g++ -O3 -std=c++20 tools/min_main.cpp -o tools/min-host
# Replace the inode only after linking, so an ongoing --full run can finish.
g++ -O3 -std=c++20 tools/ida_host.cpp -o tools/ida-host.new
mv tools/ida-host.new tools/ida-host
cc -O3 -std=c99 tools/oracle.c -o tools/oracle
if [[ -n ${HOST_LIMIT:-} ]]; then
    ./tools/host "$HOST_LIMIT" | tee tools/host-partial.log
else
    ./tools/host | tee tools/host.log
fi
if [[ ${IDA_FULL:-0} == 1 ]]; then
    ./tools/ida-host --full | tee tools/ida-full.log
else
    ./tools/ida-host --sample "${IDA_SAMPLE:-100000}" | tee "${IDA_LOG:-tools/ida-host.log}"
fi
python3 tools/target.py "$@" | tee "${TARGET_LOG:-tools/target-current.log}"
