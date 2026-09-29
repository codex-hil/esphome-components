#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror "$root/tests/rtd_math.cpp" -o "$tmp/math"
"$tmp/math"
c++ -std=c++17 -Wall -Wextra -Werror -I"$root/tests/rtd_stubs" \
  "$root/tests/rtd_component.cpp" "$root/components/rtd/rtd.cpp" -o "$tmp/component"
"$tmp/component"
python3 "$root/tests/test_rtd_schema.py"
