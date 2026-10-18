#!/usr/bin/env python3
"""Generate a compile-only native-API fixture, including string mask lambdas."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
text = (root / 'examples/switch-matrix-32-2.yaml').read_text().split('# Native API access')[0]
text = text.replace('path: ../components', f'path: {root}/components')
text = text.replace('  name: switch-matrix', '  name: switch-matrix\n  build_path: ' + str(root / 'examples/.esphome/build/switch-matrix'))
text += '''
wifi:
  ssid: COMPILE_ONLY
  password: compile-only-password
api:
  actions:
    - action: set_matrix_masks
      variables:
        mask_a: string
        mask_b: string
      then:
        - switch_matrix_32_2.set_masks:
            id: matrix
            mask_a: !lambda 'return mask_a;'
            mask_b: !lambda 'return mask_b;'
'''
folder = root / 'artifacts/switch-matrix-build'
folder.mkdir(parents=True, exist_ok=True)
path = folder / 'api-compile-only.yaml'
path.write_text(text)
print(path)
