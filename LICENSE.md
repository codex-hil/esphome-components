# Licenses and attribution

Imported files retain their original authorship and notices. `CODEOWNERS` in
source files is preserved; the import does not appoint those users as maintainers
of this repository or assert they reviewed this collection.

Files originating from `gkasprow/esphome` and `wizath/esphome` are covered by their
ESPHome repository license: runtime C/C++ under GPLv3 and Python/other parts under
MIT, with any file-specific notices retained. The complete source license is in
[licenses/ESPHOME-LICENSE](licenses/ESPHOME-LICENSE). Exact repository/revision/path
and file hashes are recorded in [catalog/components.json](catalog/components.json).
Individual library notices (including third-party driver headers) also apply.

`ads124s08_base` is a snapshot of `codex-hil/esphome-hil@631bf39`,
collected and published on the project owner's explicit instruction. That source repository has no root
LICENSE at import time; this collection does not silently assign it the fork's
license or grant additional redistribution rights.

Collection scripts/docs authored for this MODULIQ collection do not alter
the licenses of imported code. No upstream examples containing environment
configuration, firmware, credentials or toolchain installations were imported.

## Sinara slow-control components

`components/sinara_gpio_endpoint`, `components/sinara_sampler`,
`components/sinara_zotino`, their new tests and examples are distributed under
GNU GPL version 3 or later; see [full text](licenses/SINARA-GPL-3).
Copyright 2026 codex-hil contributors. The Zotino AD5372 protocol implementation
was adapted from M-Labs ARTIQ `zotino.py` and `ad53xx.py` at revision
`7d7b85f377e0c172ae3496f0c03cfb6cb85ba022` (LGPLv3+ source project;
see https://github.com/m-labs/artiq/blob/7d7b85f377e0c172ae3496f0c03cfb6cb85ba022/LICENSE).
This GPL distribution preserves attribution; it does not change the license
of ARTIQ upstream. Pin maps also follow that revision's `gateware/eem.py`.
