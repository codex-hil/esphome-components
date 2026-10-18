"""Download pinned source dependencies for offline tests; no hardware access."""
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
manifest = json.loads((ROOT/'evidence/source-commits.json').read_text())


def checkout(name, destination, commit=None):
    info = manifest[name]
    commit = commit or info['commit']
    if destination.exists():
        actual = subprocess.check_output(['git', '-C', str(destination), 'rev-parse', 'HEAD'], text=True).strip()
        if actual != commit:
            raise RuntimeError(f'{destination}: different revision; leaving untouched')
        return
    destination.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['git', 'init', '-q', str(destination)], check=True)
    subprocess.run(['git', '-C', str(destination), 'remote', 'add', 'origin', info['origin']], check=True)
    subprocess.run(['git', '-C', str(destination), 'fetch', '--depth=1', 'origin', commit], check=True)
    subprocess.run(['git', '-C', str(destination), 'checkout', '-q', '--detach', commit], check=True)


for name in ('migen', 'misoc', 'litex'):
    target = ROOT/'work/upstream-stm/deps'/name
    checkout(name, target)
    if name == 'migen':
        # Unit tests use the same checked-out dependency as the integration test.
        reference = ROOT/'references/migen'
        if not reference.exists():
            reference.parent.mkdir(exist_ok=True)
            reference.symlink_to(target, target_is_directory=True)
checkout('stm-gateware', ROOT/'work/upstream-stm/gateware',
         '46f14f81d6a29c664f72d8c7373f4d346b43970d')
checkout('artiq', ROOT/'references/artiq')
print('Pinned references ready; upstream licenses remain in each checkout.')
