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

assert module.PUBLISHED_SHA == "c1b3eb31e82bcc06a078bc10e10f342aa8329e25"
assert module.OLD_SHA == "3077dd6f0b803a523f7ab41d6203dfce9cfde978"
assert module.NEW_SHA == "c978dfcd6a9565d712940b67694f49f8505dd62c"
assert module.PUBLISHED_SHA != module.OLD_SHA != module.NEW_SHA

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

# The actual preview path must surface errors without waiting or tapping.
saved_board_fixture = object.__new__(module.Upgrade)
candidate_board = tree(base)
def fixed_preview_wait(name, predicate, **kwargs):
    assert name == "saved-preview-0-or-error"
    assert predicate(candidate_board), "Unexpected state was not detected"
    return candidate_board
saved_board_fixture.wait = fixed_preview_wait
assert module.Upgrade.board(saved_board_fixture, "saved-preview-0") is candidate_board
for forbidden in ("Keep your current game?", "Unsaved progress", "Storage error"):
    candidate_board = tree([forbidden])
    try:
        module.Upgrade.board(saved_board_fixture, "saved-preview-0")
    except RuntimeError as exc:
        assert forbidden in str(exc)
    else:
        raise AssertionError("Saved-game modal or storage failure was accepted")
candidate_board = tree(base)

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
# The API30 test emulator must inventory the actual SDL Android files/ root.
# Do not use a nested shell command: adb shell sh -c may separate cd and find.
data_root = "/data/user/0/" + module.PACKAGE + "/files"
profile_path = data_root + "/profile.dat"
session_path = data_root + "/session.dat"
expected_find = ("find", data_root, "-type", "f", "-exec",
                 "sha256sum", "{}", "+")
valid_inventory = ("a" * 64 + "  " + profile_path + "\n" +
                   "b" * 64 + "  " + session_path)
fake_private = object.__new__(module.Upgrade)
inventory_calls = []
def fake_private_shell(*parts):
    inventory_calls.append(parts)
    if parts == ("id", "-u"):
        return "0"
    assert parts == expected_find, "Unsafe or relative private inventory command"
    return valid_inventory
fake_private.shell = fake_private_shell
assert module.Upgrade.private_files(fake_private) == {
    profile_path: "a" * 64, session_path: "b" * 64}
assert inventory_calls == [("id", "-u"), expected_find]

nonroot = object.__new__(module.Upgrade)
nonroot_calls = []
def nonroot_shell(*parts):
    nonroot_calls.append(parts)
    assert parts == ("id", "-u"), "Unprivileged shell must not read private files"
    return "2000"
nonroot.shell = nonroot_shell
assert module.Upgrade.private_files(nonroot) is None
assert nonroot_calls == [("id", "-u")]

for bad_inventory in (
    "", "a" * 64 + "  " + data_root + "/profile.dat.bak",
    "a" * 64 + "  files/profile.dat",
    "a" * 64 + "  " + profile_path + "\n" + "b" * 64 + "  /tmp/foreign",
    "unexpected output"):
    invalid = object.__new__(module.Upgrade)
    def bad_shell(*parts, response=bad_inventory):
        if parts == ("id", "-u"):
            return "0"
        assert parts == expected_find
        return response
    invalid.shell = bad_shell
    try:
        module.Upgrade.private_files(invalid)
    except RuntimeError:
        pass
    else:
        raise AssertionError("Missing, malformed or foreign private files accepted")

missing = object.__new__(module.Upgrade)
def missing_shell(*parts):
    if parts == ("id", "-u"):
        return "0"
    assert parts == expected_find
    raise RuntimeError("ADB failed: private directory missing")
missing.shell = missing_shell
try:
    module.Upgrade.private_files(missing)
except RuntimeError as exc:
    assert "private directory missing" in str(exc)
else:
    raise AssertionError("ADB find failure suppressed")

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


# Storage faults must be surfaced by the first fresh UI snapshot, not timed out.
storage_tree = ET.fromstring('<hierarchy><node enabled="true" '
                             'content-desc="Storage error"/></hierarchy>')
storage_fixture = object.__new__(module.Upgrade)
storage_calls = []
def storage_shell(*args):
    assert args == ("am", "start", "-W", "-n", module.ACTIVITY)
storage_fixture.shell = storage_shell
def storage_wait(label, predicate, **kwargs):
    storage_calls.append(label)
    assert label == "opened" and predicate(storage_tree)
    return storage_tree
storage_fixture.wait = storage_wait
try:
    module.Upgrade.start(storage_fixture)
except RuntimeError as exc:
    assert "Storage error at startup" in str(exc)
else:
    raise AssertionError("Startup storage error was not detected promptly")
assert storage_calls == ["opened"]

# Metadata must describe app-owned paths and a verified SELinux context.
metadata_root = "/data/user/0/" + module.PACKAGE
meta_files = {metadata_root + "/files/profile.dat": "a" * 64,
              metadata_root + "/files/games/example.dat": "b" * 64}
meta_fixture = object.__new__(module.Upgrade)
meta_calls = []
def metadata_shell(*args):
    meta_calls.append(args)
    if args == ("id", "-u"):
        return "0"
    if args == ("pm", "list", "packages", "-U", module.PACKAGE):
        return "package:" + module.PACKAGE + " uid:10123"
    if args[:3] == ("stat", "-c", "%u:%g:%a"):
        return "10123:10123:600" if args[3].endswith(".dat") else "10123:10123:700"
    if args[:2] == ("ls", "-Zd"):
        return "u:object_r:app_data_file:s0:c123,c456 " + args[2]
    raise AssertionError("Unexpected metadata command: %r" % (args,))
meta_fixture.shell = metadata_shell
metadata = module.Upgrade.private_metadata(meta_fixture, meta_files)
assert metadata["package_uid"] == 10123
assert len(metadata["paths"]) == 5
assert all(entry["selinux"].startswith("u:object_r:")
           for entry in metadata["paths"].values())
for fault, bad_value in (
    (("pm", "list", "packages", "-U", module.PACKAGE), "unknown"),
    (("stat", "-c", "%u:%g:%a", metadata_root + "/files/profile.dat"), "bad"),
    (("ls", "-Zd", metadata_root + "/files/profile.dat"), "no-context")):
    def broken_meta_shell(*args, expected=fault, bad=bad_value):
        return bad if args == expected else metadata_shell(*args)
    meta_fixture.shell = broken_meta_shell
    try:
        module.Upgrade.private_metadata(meta_fixture, meta_files)
    except RuntimeError:
        pass
    else:
        raise AssertionError("Invalid private metadata accepted")
meta_fixture.shell = metadata_shell

# Reopening the original version is a mandatory gate before install -r.
import tempfile
with tempfile.TemporaryDirectory() as temp:
    cold = object.__new__(module.Upgrade)
    cold.out = Path(temp)
    cold_stops = []
    cold.shell = lambda *args: cold_stops.append(args)
    cold.start = lambda: None
    cold.settings = lambda: "Theme: Light"
    cold.saved_previews = lambda boards: sorted(snapshots)
    cold.history = lambda: ["Interrupted attempt"]
    module.Upgrade.cold_reopen(
        cold, snapshots, "Theme: Light", sorted(snapshots), ["Interrupted attempt"])
    assert cold_stops == [("am", "force-stop", module.PACKAGE)] * 2
    assert (cold.out / "old-cold-reopen.json").is_file()
    module.Upgrade.cold_reopen(
        cold, snapshots, "Theme: Light", sorted(snapshots),
        ["Interrupted attempt"], version="1.4.1", code=2, label="new")
    verified = json.loads((cold.out / "new-cold-reopen.json").read_text())
    assert verified["source"] == module.NEW_SHA
    assert verified["version"] == "1.4.1" and verified["code"] == 2
    assert verified["two_unique_saved_boards"] == "PASS"
    assert verified["notes_settings_history"] == "PASS"
    try:
        module.Upgrade.cold_reopen(
            cold, snapshots, "Theme: Dark", sorted(snapshots), ["Interrupted attempt"])
    except RuntimeError as exc:
        assert "cold reopen" in str(exc)
    else:
        raise AssertionError("Incorrect original cold-reopen settings passed")

print("PASS: pinned upgrade identity, two cold reopens, dialogs and private metadata")
