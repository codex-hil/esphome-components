#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/include/esphome"
ln -s "$root/components" "$tmp/include/esphome/components"
c++ -std=c++17 -Wall -Wextra -Werror \
  -I"$root/tests/addressed_spi_stubs" -I"$root/tests/cpld_stubs" \
  -I"$root/tests/rtd_stubs" -I"$tmp/include" \
  "$root/tests/addressed_spi.cpp" "$root/components/addrspi/addrspi.cpp" \
  "$root/components/addrspi2/addrspi2.cpp" "$root/components/mcp3208/mcp3208.cpp" \
  "$root/components/dacx0504/dacx0504.cpp" \
  "$root/components/spi_shift_register/spi_shift_register.cpp" -o "$tmp/test"
"$tmp/test"
