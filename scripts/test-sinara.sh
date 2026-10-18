#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
mkdir -p work
c++ -std=c++17 -Wall -Wextra -Werror -pedantic -fsanitize=undefined \
  -Icomponents/sinara_gpio_endpoint tests/sinara_sampler_core.cpp -o work/sinara-sampler-core-test
./work/sinara-sampler-core-test
cargo test --manifest-path firmware/sinara-stm-gateway/Cargo.toml
python3 tests/test_sinara_sampler_esphome.py
