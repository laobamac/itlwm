#!/bin/bash
# Host checks only: these do not exercise firmware, DMA, or the Wi-Fi radio.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
mkdir -p build/host-tests
python3 -m unittest discover -s tests -v
python3 scripts/verify_meteorlake.py itlwm/firmware
for name in pnvm mld_mac mld_data phy_v4 scan_v16; do
    binary="build/host-tests/test_${name}"
    clang++ -std=c++11 -Wall -Wextra -Werror -fsanitize=address,undefined \
        "tests/test_${name}.cpp" -o "$binary"
    if [[ "$name" == pnvm ]]; then
        "$binary" itlwm/firmware/*.pnvm
    else
        "$binary"
    fi
done
