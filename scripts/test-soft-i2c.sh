#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
hil_root=${CPLD_HIL_ROOT:-$root/../esphome-hil}
if [[ ${HIL_NIX_ACTIVE:-} != 1 ]]; then
  exec "$hil_root/scripts/nix-exec.sh" bash "$0" "$@"
fi
exec "$root/tests/run-soft-i2c-software.sh" "$HIL_ESPHOME/bin/python3"
