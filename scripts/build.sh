#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
python3 "$root/scripts/verify.py"
python3 -c 'from esphome.const import __version__; assert __version__ == "2026.9.0", "Use ESPHome 2026.9.0 for this baseline"'
exec esphome compile "${1:-$root/examples/bridge-adc.yaml}"
