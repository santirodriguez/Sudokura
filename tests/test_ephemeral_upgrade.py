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
# Two equal row labels must still be distinct selections; opening a game
# requires the separately enabled Continue game action, then Save and leave.
def saved_tree(selected=0, second_enabled=True):
    action = "true" if second_enabled else "false"
    return ET.fromstring(
        '<hierarchy>'
        '<node content-desc="Classic, Medium, In progress" enabled="true" '
        'clickable="true" selected="%s" bounds="[10,20][80,40]"/>'
        '<node content-desc="Classic, Medium, In progress" enabled="true" '
        'clickable="true" selected="%s" bounds="[10,40][80,60]"/>'
        '<node content-desc="Continue game" enabled="%s" clickable="true" '
        'bounds="[90,70][180,100]"/>'
        '</hierarchy>' % ("true" if selected == 0 else "false",
                        "true" if selected == 1 else "false", action)
    )

assert len(module.saved_rows(saved_tree())) == 2
assert module.selected_saved_index(saved_tree(0), 2) == 0
assert module.selected_saved_index(saved_tree(1), 2) == 1
assert not module.Upgrade.find(object.__new__(module.Upgrade),
                               saved_tree(second_enabled=False), "Continue game")
for bad_tree in (
    ET.fromstring('<hierarchy><node content-desc="Classic, Medium, In progress" '
                   'enabled="true" clickable="true" selected="true"/></hierarchy>'),
    ET.fromstring('<hierarchy><node content-desc="Classic, Medium, In progress" '
                   'enabled="true" clickable="true" selected="true"/>'
                   '<node content-desc="Classic, Medium, In progress" '
                   'enabled="true" clickable="true" selected="true"/></hierarchy>')
):
    try:
        module.selected_saved_index(bad_tree, 2)
    except RuntimeError:
        pass
    else:
        raise AssertionError("Invalid or ambiguous saved-game selection accepted")

different = base[:]
different[10] = "Row 2, column 2. Entered 4"
different[11] = "Row 2, column 3. Empty. Notes 7"
snapshots = (module.normalized_cells(tree(filled)),
             module.normalized_cells(tree(different)))
state = {"screen": "home", "selected": 0, "opened": None}
actions = []
fake_games = object.__new__(module.Upgrade)
def games_home():
    state["screen"] = "home"
def games_tap(label, **kwargs):
    if label != "Saved games" or state["screen"] != "home":
        raise AssertionError("Unexpected saved-game navigation: " + label)
    state["screen"] = "list"
    actions.append("Saved games")
def games_wait(name, predicate, **kwargs):
    if state["screen"] != "list":
        raise AssertionError("Saved list not visible for " + name)
    found_tree = saved_tree(state["selected"])
    if not predicate(found_tree):
        raise AssertionError("Selected saved-game row predicate failed: " + name)
    return found_tree
def games_tap_node(node):
    if state["screen"] != "list":
        raise AssertionError("Tap outside saved-games list")
    label = node.get("content-desc")
    if label == "Continue game":
        if node.get("enabled") != "true":
            raise AssertionError("Disabled Continue game accepted")
        state["opened"] = state["selected"]
        state["screen"] = "board"
        actions.append("Continue game")
    elif "In progress" in label:
        bounds = node.get("bounds")
        if bounds not in ("[10,20][80,40]", "[10,40][80,60]"):
            raise AssertionError("Unknown row")
        state["selected"] = 0 if bounds == "[10,20][80,40]" else 1
        actions.append("row %d" % state["selected"])
    else:
        raise AssertionError("Unknown saved-game control: " + label)
def games_board(name):
    if state["screen"] != "board":
        raise AssertionError("Board not opened by Continue game")
    return tree(filled if state["opened"] == 0 else different)
def games_to_home():
    if state["screen"] != "board":
        raise AssertionError("Save and leave without board")
    actions.append("Save and leave")
    state["screen"] = "home"
fake_games.home = games_home
fake_games.tap = games_tap
fake_games.wait = games_wait
fake_games.tap_node = games_tap_node
fake_games.board = games_board
fake_games.to_home = games_to_home
assert module.Upgrade.saved_previews(fake_games, snapshots) == sorted(snapshots)
assert actions == ["Saved games", "row 0", "Continue game", "Save and leave",
                   "Saved games", "row 1", "Continue game", "Save and leave"]
fake_games.board = lambda name: tree(filled)
try:
    module.Upgrade.saved_previews(fake_games, snapshots)
except RuntimeError as exc:
    assert "unique boards" in str(exc)
else:
    raise AssertionError("Duplicate board accepted for two identical row labels")

print("PASS: semantic row selection, Continue game activation and unique board fixtures")
