#!/usr/bin/env bash
# Called by the pinned Nix entrypoint; CI may supply its explicit job-local Python.
# This is software testing, never environment commissioning or hardware validation.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
[[ $# == 1 && -x $1 ]] || { echo 'Usage: run-cpld-software.sh EXPLICIT_ESPHOME_PYTHON' >&2; exit 2; }
test_python=$1
"$test_python" -c 'from esphome.const import __version__; assert __version__ == "2026.9.0"'
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/include/esphome"
ln -s "$root/components" "$tmp/include/esphome/components"
c++ -std=c++17 -Wall -Wextra -Werror -DUSE_MODULIQ_CPLD_GPIO \
  -I"$root/tests/cpld_stubs" -I"$tmp/include" \
  "$root/tests/cpld_components.cpp" "$root/components/addrspi/addrspi.cpp" \
  "$root/components/moduliq_cpld_gpio/moduliq_cpld_gpio.cpp" \
  "$root/components/moduliq_cpld_i2c/moduliq_cpld_i2c.cpp" \
  "$root/components/moduliq_cpld_flash/moduliq_cpld_flash.cpp" -o "$tmp/components"
"$tmp/components"
c++ -std=c++17 -Wall -Wextra -Werror -I"$root/tests/cpld_stubs" \
  -c "$root/components/moduliq_cpld_i2c/moduliq_cpld_i2c.cpp" -o "$tmp/readout.o"
"$test_python" "$root/tests/test_cpld_schema.py"
