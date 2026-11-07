#!/usr/bin/env python3
"""Fail-closed parser and identity tests for disposable upgrade acceptance."""
import importlib.util
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

script = Path(__file__).resolve().parents[1] / "android/scripts/ephemeral_upgrade.py"
sys.path.insert(0, str(script.parent))
spec = importlib.util.spec_from_file_location("upgrade", script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

def tree(labels):
    nodes = "".join('<node content-desc="%s" enabled="true"/>' % s for s in labels)
    return ET.fromstring("<hierarchy>" + nodes + "</hierarchy>")

base = ["Row %d, column %d. Empty" % (r, c)
        for r in range(1, 10) for c in range(1, 10)]
assert len(module.parse_cells(tree(base))) == 81
assert len(module.normalized_cells(tree(base))) == 81
filled = base[:]
filled[0] = "Row 1, column 1. Entered 3. Selected"
filled[1] = "Row 1, column 2. Empty. Notes 7"
content = module.normalized_cells(tree(filled))
assert content[0] == ((1, 1), "Entered 3")
assert content[1] == ((1, 2), "Empty. Notes 7")
try:
    module.normalized_cells(tree(filled[:-1]))
except RuntimeError:
    pass
else:
    raise AssertionError("Partial 80-cell board accepted")
try:
    module.parse_cells(tree([base[0], base[0]]))
except RuntimeError:
    pass
else:
    raise AssertionError("Duplicate semantic cell accepted")
valid = "Signer #1 certificate SHA-256 digest: " + "a" * 64
assert module.signing_cert(valid) == "a" * 64
for bad in ("", valid + "\n" + valid, "Signer #2 certificate SHA-256 digest: " + "b" * 64):
    try:
        module.signing_cert(bad)
    except RuntimeError:
        pass
    else:
        raise AssertionError("Ambiguous signer accepted")
assert module.parse_hashes("a" * 64 + "  files/profile.dat") == {
    "files/profile.dat": "a" * 64}
for bad in ("garbage", "a" * 64 + "  files/profile.dat\n" + "b" * 64 + "  files/profile.dat"):
    try:
        module.parse_hashes(bad)
    except RuntimeError:
        pass
    else:
        raise AssertionError("Invalid file inventory accepted")
assert not module.package_present("", module.PACKAGE)
assert module.package_present("package:org.cajapersonal.sudokura\n", module.PACKAGE)
assert not module.package_present("package:org.cajapersonal.sudokura.debug\n", module.PACKAGE)
assert not module.package_present("package:org.example.test\n", module.PACKAGE)
assert module.PUBLIC_CERT != "a" * 64

# Settings exposes a composed accessibility label; a bare "Back" must NOT
# appear to be a valid semantic target. Exercise the actual settings method.
settings_tree = ET.fromstring(
    '<hierarchy><node enabled="true" content-desc="Theme: Light"/>'
    '<node enabled="true" clickable="true" content-desc="Back: Back"/>'
    '</hierarchy>'
)
fake = object.__new__(module.Upgrade)
labels = []
home_calls = []
fake.home = lambda: home_calls.append("home")
def fake_tap(label, **kwargs):
    if label != "Settings" and not module.Upgrade.find(fake, settings_tree, label):
        raise AssertionError("Unknown Settings accessibility label: " + label)
    labels.append(label)
fake.tap = fake_tap
def fake_wait(name, predicate, **kwargs):
    assert name == "settings" and predicate(settings_tree)
    return settings_tree
fake.wait = fake_wait
assert module.Upgrade.find(fake, settings_tree, "Back") == []
assert len(module.Upgrade.find(fake, settings_tree, "Back: Back")) == 1
assert module.Upgrade.settings(fake) == "Theme: Light"
assert labels == ["Settings", "Back: Back"]
assert home_calls == ["home", "home"]
print("PASS: board cells, notes, signer identity, inventory and Settings Back regression")
