#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
hil_root=${CPLD_HIL_ROOT:-$root/../esphome-hil}
if [[ ${HIL_NIX_ACTIVE:-} != 1 ]]; then
  exec "$hil_root/scripts/nix-exec.sh" bash "$0" "$@"
fi
[[ -x "$HIL_ESPHOME/bin/python3" ]] || { echo 'Pinned Nix ESPHome required' >&2; exit 2; }
exec "$root/tests/run-cpld-software.sh" "$HIL_ESPHOME/bin/python3"
