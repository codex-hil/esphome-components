"""Offline source materialization test; pass an existing pinned upstream checkout."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch

root = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('zt_source', root/'components/zerotier/source.py')
source = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source)
checkout = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory() as tmp:
    destination = Path(tmp)/'zerotier_native'
    with patch.object(source.CORE, 'relative_build_path', return_value=str(destination)), patch.object(source.git, 'clone_or_update', return_value=(checkout, None)) as fetch:
        assert source.prepare_component() == destination
        assert json.loads((destination/'.recipe.json').read_text())['upstream'] == source.REVISION
        assert b'ESP_PLATFORM' in (destination/'upstream/node/Mutex.hpp').read_bytes()
        assert '__ORDER_LITTLE_ENDIAN__' in (destination/'upstream/node/Constants.hpp').read_text()
        assert source.prepare_component() == destination
        assert fetch.call_count == 1
    unowned = Path(tmp)/'unowned'
    unowned.mkdir()
    with patch.object(source.CORE, 'relative_build_path', return_value=str(unowned)):
        try:
            source.prepare_component()
        except ValueError:
            pass
        else:
            raise AssertionError('Unowned build directory was accepted')
print('PASS: pinned upstream patch, cached reuse, unowned-directory protection')
