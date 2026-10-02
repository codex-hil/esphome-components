#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
[[ $# == 1 && -x $1 ]] || { echo 'Usage: run-soft-i2c-software.sh EXPLICIT_ESPHOME_PYTHON' >&2; exit 2; }
test_python=$1
"$test_python" -c 'from esphome.const import __version__; assert __version__ == "2026.9.0"'
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/include/esphome"
ln -s "$root/components" "$tmp/include/esphome/components"
c++ -std=c++17 -Wall -Wextra -Werror -DUSE_MODULIQ_CPLD_GPIO \
  -I"$root/tests/cpld_stubs" -I"$tmp/include" \
  "$root/tests/soft_i2c_component.cpp" "$root/components/addrspi/addrspi.cpp" \
  "$root/components/moduliq_cpld_gpio/moduliq_cpld_gpio.cpp" \
  "$root/components/moduliq_cpld_i2c/moduliq_cpld_i2c.cpp" \
  "$root/components/moduliq_cpld_flash/moduliq_cpld_flash.cpp" \
  "$root/components/moduliq_cpld_soft_i2c/moduliq_cpld_soft_i2c.cpp" -o "$tmp/test"
"$tmp/test"
"$test_python" "$root/tests/test_soft_i2c_schema.py"
