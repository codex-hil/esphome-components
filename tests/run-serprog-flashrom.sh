#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
[[ $# == 3 && -x $1 && -x $2 ]] || { echo 'Usage: run-serprog-flashrom.sh NIX_PYTHON FLASHROM LOG_DIRECTORY' >&2; exit 2; }
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/include/esphome"
ln -s "$root/components" "$tmp/include/esphome/components"
c++ -std=c++17 -Wall -Wextra -Werror -DUSE_MODULIQ_CPLD_GPIO \
  -I"$root/tests/cpld_stubs" -I"$tmp/include" "$root/tests/serprog_flashrom_server.cpp" \
  "$root/components/moduliq_serprog/protocol.cpp" "$root/components/moduliq_serprog/moduliq_serprog.cpp" \
  "$root/components/addrspi/addrspi.cpp" "$root/components/moduliq_cpld_gpio/moduliq_cpld_gpio.cpp" \
  "$root/components/moduliq_cpld_flash/moduliq_cpld_flash.cpp" "$root/components/moduliq_cpld_i2c/moduliq_cpld_i2c.cpp" -o "$tmp/server"
"$1" "$root/tests/test_serprog_flashrom.py" "$tmp/server" "$2" "$3"
