#!/usr/bin/env bash
# Pin the client to the same nixpkgs input as esphome-hil, without changing that environment.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
hil_root=${CPLD_HIL_ROOT:-$root/../esphome-hil}
if [[ ${HIL_NIX_ACTIVE:-} != 1 ]]; then
  exec "$hil_root/scripts/nix-exec.sh" bash "$0" "$@"
fi
nix_bin=$(command -v nix || true)
if [[ -z $nix_bin && -x /nix/var/nix/profiles/default/bin/nix ]]; then
  nix_bin=/nix/var/nix/profiles/default/bin/nix
fi
[[ -n $nix_bin ]] || { echo 'Nix required for the pinned flashrom client' >&2; exit 2; }
mkdir -p "$root/artifacts"
"$nix_bin" --extra-experimental-features 'nix-command flakes' build \
  github:NixOS/nixpkgs/1bc55b9def8165e82073919945c3239903fe4dc2#flashrom \
  --out-link "$root/artifacts/serprog-flashrom"
exec "$root/tests/run-serprog-flashrom.sh" "$HIL_ESPHOME/bin/python3" \
  "$root/artifacts/serprog-flashrom/bin/flashrom" "${1:-$root/artifacts/serprog-flashrom-test}"
