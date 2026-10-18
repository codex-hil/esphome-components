# Licenses and provenance

The Sinara C++ runtime in `components/` and C++ tests use **GPL-3.0-only**.
Python schemas, scripts, gateware, Rust gateway, YAML examples and documentation
use **MIT**. Source headers identify these licenses; full texts are in
[licenses/](licenses/). These terms apply to the new Sinara implementation,
not to third-party projects or other branches' imported components.

The implementation originated in the project owner's Unified Sinara Slow
Control workspace and is published on their explicit instruction. Source
paths and original content hashes are recorded in
[reports/sinara-sampler-provenance.json](reports/sinara-sampler-provenance.json).
It independently implements the device/register protocols documented by
ARTIQ, the STM gateway and LiteX. No ARTIQ, ELHEP, Migen or LiteX source tree
is vendored here. Dependencies retain their own licenses.

`zotino_core.h` is a shared endpoint dependency from the same locally authored
implementation. Its inclusion does not claim a tested Zotino ESPHome component
in this publication. The older MODULIQ collection remains on its existing
branches, with its original attribution and notices.
