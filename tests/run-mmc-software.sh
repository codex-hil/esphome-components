#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/include/esphome"
ln -s "$root/components" "$tmp/include/esphome/components"
c++ -std=c++17 -Wall -Wextra -Werror -I"$root/tests/mmc_stubs" -I"$root/tests/cpld_stubs" -I"$tmp/include" \
 "$root/tests/mmc5983_spi.cpp" "$root/components/mmc5983_spi/mmc5983_spi.cpp" -o "$tmp/mmc"
"$tmp/mmc"
echo 'MMC5983 software regression PASS (simulated bus only)'
