#!/usr/bin/env python3
"""Offline provenance/content/syntax checks; Python standard library only."""
import ast
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
manifest = json.loads((ROOT / "catalog/components.json").read_text())
count = 0
for entry in manifest["components"]:
    assert len(entry["source"]["revision"]) == 40, entry["name"]
    directory = ROOT / entry["path"]
    expected = entry["files_sha256"]
    actual = {str(f.relative_to(directory)) for f in directory.rglob("*")
              if f.is_file() and "__pycache__" not in f.parts}
    assert actual == set(expected), (entry["name"], actual ^ set(expected))
    for name, sha in expected.items():
        file = directory / name
        assert hashlib.sha256(file.read_bytes()).hexdigest() == sha, file
        if file.suffix == ".py":
            ast.parse(file.read_text(), filename=str(file))
        count += 1
print(f"PASS: {len(manifest['components'])} entries, {count} pinned source files; Python syntax valid")
