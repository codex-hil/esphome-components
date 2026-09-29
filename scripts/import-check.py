#!/usr/bin/env python3
"""Schema-import check only: does not claim codegen, compile or hardware support."""
import importlib
import json
from pathlib import Path
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
if len(sys.argv) > 1:
    from esphome.loader import install_meta_finder
    install_meta_finder(ROOT / "components")
    name = sys.argv[1]
    for f in sorted((ROOT / "components" / name).rglob("*.py")):
        parts = list(f.relative_to(ROOT / "components").with_suffix("").parts)
        if parts[-1] == "__init__": parts.pop()
        importlib.import_module("esphome.components." + ".".join(parts))
    sys.exit(0)
from esphome.const import __version__
result = {"esphome": __version__, "scope": "Python schema import only", "components": {}}
for directory in sorted((ROOT / "components").iterdir()):
    if not directory.is_dir(): continue
    p = subprocess.run([sys.executable, __file__, directory.name], text=True, capture_output=True)
    result["components"][directory.name] = {"result": "PASS" if p.returncode == 0 else "FAIL"}
    if p.returncode: result["components"][directory.name]["error"] = p.stderr.strip().splitlines()[-1]
print(json.dumps(result, indent=2))
# Inventory all failures without misrepresenting them as a passing test.
sys.exit(any(v["result"] == "FAIL" for v in result["components"].values()))
