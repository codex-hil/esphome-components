"""Materialize a pinned upstream + reviewed patch in the generated build tree."""
import filecmp
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

from esphome import git
from esphome.core import CORE, TimePeriodSeconds

UPSTREAM = "https://github.com/zerotier/ZeroTierOne.git"
REVISION = "899352e38405968516bb12a770f0ac02f6058fa8"


def prepare_component():
    source = Path(__file__).parent
    files = sorted(p for p in (source / "idf").rglob("*") if p.is_file())
    digest = hashlib.sha256(REVISION.encode() + (source / "upstream.patch").read_bytes())
    for path in files:
        digest.update(str(path.relative_to(source)).encode())
        digest.update(path.read_bytes())
    stamp = {"upstream": REVISION, "recipe_sha256": digest.hexdigest()}
    dest = Path(CORE.relative_build_path("zerotier_native"))
    if (dest / ".recipe.json").exists():
        if json.loads((dest / ".recipe.json").read_text()) == stamp:
            return dest
    elif dest.exists():
        raise ValueError("Refusing to replace an unowned zerotier_native directory")
    checkout, _ = git.clone_or_update(url=UPSTREAM, ref=REVISION,
        refresh=TimePeriodSeconds(days=3650), domain="zerotier")
    actual = subprocess.check_output(["git", "-C", str(checkout), "rev-parse", "HEAD"], text=True).strip()
    if actual != REVISION:
        raise ValueError("Unexpected ZeroTier upstream revision")
    dest.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="zt-native-", dir=dest.parent) as tmp:
        staging = Path(tmp) / "component"
        shutil.copytree(source / "idf", staging)
        upstream = staging / "upstream"
        upstream.mkdir()
        archive = subprocess.check_output(["git", "-C", str(checkout), "archive", REVISION,
            "node", "include", "osdep", "ext", "version.h", "LICENSE.txt", "LICENSE-MPL.txt"])
        with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
            tar.extractall(upstream, filter="data")
        subprocess.run(["git", "init", "-q", str(upstream)], check=True)
        subprocess.run(["git", "-C", str(upstream), "apply", "--check", str(source / "upstream.patch")], check=True)
        subprocess.run(["git", "-C", str(upstream), "apply", str(source / "upstream.patch")], check=True)
        shutil.rmtree(upstream / ".git")
        (staging / ".recipe.json").write_text(json.dumps(stamp, indent=2) + "\n")
        if dest.exists():
            # Keep unchanged translation units' mtimes for incremental IDF builds.
            for path in staging.rglob("*"):
                old = dest / path.relative_to(staging)
                if path.is_file() and old.is_file() and filecmp.cmp(path, old, shallow=False):
                    shutil.copystat(old, path)
            shutil.rmtree(dest)  # Only our generated, marked component.
        staging.rename(dest)
    return dest
