#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
[[ $# == 1 && -x $1 ]] || { echo 'Usage: run-serprog-software.sh PINNED_ESPHOME_PYTHON' >&2; exit 2; }
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/include/esphome"
ln -s "$root/components" "$tmp/include/esphome/components"
flags=(-std=c++17 -Wall -Wextra -Werror -DUSE_MODULIQ_CPLD_GPIO -I"$root/tests/cpld_stubs" -I"$tmp/include")
c++ "${flags[@]}" "$root/tests/serprog_protocol.cpp" "$root/components/moduliq_serprog/protocol.cpp" -o "$tmp/protocol"
"$tmp/protocol"
c++ "${flags[@]}" "$root/tests/serprog_component.cpp" \
  "$root/components/moduliq_serprog/protocol.cpp" "$root/components/moduliq_serprog/moduliq_serprog.cpp" \
  "$root/components/addrspi/addrspi.cpp" "$root/components/moduliq_cpld_gpio/moduliq_cpld_gpio.cpp" \
  "$root/components/moduliq_cpld_flash/moduliq_cpld_flash.cpp" "$root/components/moduliq_cpld_i2c/moduliq_cpld_i2c.cpp" -o "$tmp/component"
"$tmp/component"
"$1" "$root/tests/test_serprog_schema.py"
