#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/include/esphome"
ln -s "$root/components" "$tmp/include/esphome/components"
c++ -std=c++17 -fsanitize=address,undefined -fno-omit-frame-pointer -Wall -Wextra -Werror \
 -I"$root/tests/matrix_stubs" -I"$root/tests/cpld_stubs" -I"$tmp/include" \
 "$root/tests/switch_matrix_component.cpp" "$root/components/switch_matrix_32_2/switch_matrix_32_2.cpp" -o "$tmp/test"
"$tmp/test"
