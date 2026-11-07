#!/usr/bin/env python3
"""Disposable test-key upgrade exercise; never touches a real Android device."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET

from emulator_runtime import capture_tree

PACKAGE = "org.cajapersonal.sudokura"
ACTIVITY = PACKAGE + "/org.sudokura.prototype.PrototypeActivity"
PUBLIC_CERT = "fde951e3d2ef42e41c7186ee965944aeca549d590ad080711dd3d875f82df66d"
OLD_SHA = "c1b3eb31e82bcc06a078bc10e10f342aa8329e25"
NEW_SHA = "810888803430d0664c6d02e1b49f67b5e590141d"
CELL = re.compile(r"^Row ([1-9]), column ([1-9])\. (.+)$")
BOUNDS = re.compile(r"^\[(\d+),(\d+)\]\[(\d+),(\d+)\]$")


def parse_cells(tree):
    result = {}
    for node in tree.iter("node"):
        match = CELL.match(node.get("content-desc", ""))
        if match:
            key = (int(match[1]), int(match[2]))
            if key in result:
                raise RuntimeError("Duplicate accessible board cell")
            result[key] = (match[3], node)
    return result


def normalized_cells(tree):
    values = parse_cells(tree)
    if len(values) != 81:
        raise RuntimeError("Expected 81 visible board cells, got %d" % len(values))
    return tuple((key, value[0].replace(". Selected", ""))
                 for key, value in sorted(values.items()))


def signing_cert(output):
    matches = re.findall(r"Signer #1 certificate SHA-256 digest:\s*([0-9a-f]{64})",
                         output, re.IGNORECASE)
    if len(matches) != 1:
        raise RuntimeError("Missing or ambiguous APK signer certificate")
    return matches[0].lower()


def package_present(output, package):
    return any(line.strip() == "package:" + package for line in output.splitlines())


def saved_rows(tree):
    return [node for node in tree.iter("node")
            if node.get("enabled") == "true"
            and node.get("clickable") == "true"
            and "In progress" in node.get("content-desc", "")]


def selected_saved_index(tree, expected_count):
    rows = saved_rows(tree)
    if len(rows) != expected_count:
        raise RuntimeError("Unexpected saved-game row count")
    selected = [idx for idx, node in enumerate(rows)
                if node.get("selected") == "true"]
    if len(selected) != 1:
        raise RuntimeError("Expected exactly one selected saved-game row")
    return selected[0]


def parse_hashes(output):
    hashes = {}
    for line in output.splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        if not match or match[2] in hashes:
            raise RuntimeError("Invalid private file inventory")
        hashes[match[2]] = match[1]
    return hashes


class Upgrade:
    def __init__(self, args):
        self.args = args
        self.out = args.output
        self.out.mkdir(parents=True, exist_ok=True)
        self.serial = args.serial
        self.adb_program = args.adb
        self.counter = 0

    def adb(self, *parts, check=True, timeout=45, binary=False):
        proc = subprocess.run([self.adb_program, "-s", self.serial, *parts],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              timeout=timeout, check=False)
        if check and proc.returncode:
            raise RuntimeError("ADB failed: %s: %s" %
                               (" ".join(parts), proc.stderr.decode(errors="replace")[:800]))
        return proc.stdout if binary else proc.stdout.decode(errors="replace").strip()

    def shell(self, *parts):
        return self.adb("shell", *parts)

    def tree(self, name):
        self.counter += 1
        try:
            return capture_tree(self.adb, self.out, "%03d-%s" % (self.counter, name))
        except (ET.ParseError, subprocess.CalledProcessError) as err:
            raise RuntimeError("Fresh Android UI hierarchy unavailable") from err

    def find(self, tree, label, prefix=False):
        return [n for n in tree.iter("node")
                if n.get("enabled") == "true"
                and (n.get("content-desc", "").startswith(label) if prefix
                     else n.get("content-desc") == label)]

    def wait(self, name, predicate, timeout=55):
        deadline = time.monotonic() + timeout
        last = []
        while time.monotonic() < deadline:
            try:
                tree = self.tree(name)
                last = [n.get("content-desc", "") for n in tree.iter("node")
                        if n.get("content-desc")]
                if predicate(tree):
                    (self.out / (name + ".png")).write_bytes(
                        self.adb("exec-out", "screencap", "-p", binary=True))
                    return tree
            except RuntimeError:
                pass
            time.sleep(1.5)
        raise RuntimeError("Timeout for %s; last visible: %r" % (name, last[:22]))

    def tap_node(self, node):
        match = BOUNDS.fullmatch(node.get("bounds", ""))
        if not match or node.get("enabled") != "true" or node.get("clickable") != "true":
            raise RuntimeError("Unsafe/disabled semantic action")
        x1, y1, x2, y2 = map(int, match.groups())
        if x2 <= x1 or y2 <= y1:
            raise RuntimeError("Invalid semantic hit rectangle")
        self.shell("input", "tap", str((x1 + x2) // 2), str((y1 + y2) // 2))
        time.sleep(.5)

    def tap(self, label, prefix=False, index=0):
        tree = self.wait("find-" + re.sub("[^a-z0-9]", "-", label.lower()),
                         lambda t: len(self.find(t, label, prefix)) > index)
        self.tap_node(self.find(tree, label, prefix)[index])

    def home(self):
        return self.wait("home", lambda t: bool(self.find(t, "Start new game")))

    def board(self, label):
        return self.wait(label, lambda t: len(parse_cells(t)) == 81)

    def seed(self, label):
        tree = self.board(label)
        cells = parse_cells(tree)
        empty = [(key, node) for key, (description, node) in sorted(cells.items())
                 if description.startswith("Empty")]
        if len(empty) < 2:
            raise RuntimeError("No two editable cells for synthetic test")
        (row, col), first = empty[0]
        givens = {(r, c): int(m[1]) for (r, c), (description, _) in cells.items()
                  if (m := re.match(r"Given ([1-9])", description))}
        occupied = {v for (r, c), v in givens.items()
                    if r == row or c == col or ((r-1)//3, (c-1)//3) ==
                       ((row-1)//3, (col-1)//3)}
        digit = next((n for n in range(1, 10) if n not in occupied), None)
        if digit is None:
            raise RuntimeError("No legal synthetic input candidate")
        self.tap_node(first)
        self.tap(str(digit))
        self.wait(label + "-entered", lambda t:
                  parse_cells(t).get((row, col), ("",))[0].startswith("Entered %d" % digit))
        note_key, note_node = empty[1]
        self.tap_node(note_node)
        self.tap("Notes")
        self.tap("7")
        self.wait(label + "-notes", lambda t:
                  "Notes 7" in parse_cells(t).get(note_key, ("",))[0])
        self.tap("Notes")
        self.tap("Save")
        done = self.board(label + "-saved")
        content = normalized_cells(done)
        if not any("Entered" in desc for _, desc in content) or not any("Notes 7" in desc for _, desc in content):
            raise RuntimeError("Saved board lacks synthetic digit or note")
        return content

    def to_home(self):
        self.tap("Menu")
        tree = self.wait("menu-exit", lambda t:
                         bool(self.find(t, "Start new game")) or
                         bool(self.find(t, "Save and leave")) or
                         bool(self.find(t, "Keep playing")))
        if self.find(tree, "Save and leave"):
            self.tap("Save and leave")
        return self.home()

    def start(self):
        self.shell("am", "start", "-W", "-n", ACTIVITY)
        tree = self.wait("opened", lambda t: bool(self.find(t, "Start new game")) or
                         bool(self.find(t, "Resume")))
        if self.find(tree, "Resume"):
            self.tap("Resume")
        return self.wait("foreground", lambda t: bool(self.find(t, "Start new game")) or
                         len(parse_cells(t)) == 81)

    def saved_previews(self, expected):
        expected = tuple(expected)
        if len(expected) != 2 or len(set(expected)) != 2 or any(
                len(cells) != 81 for cells in expected):
            raise RuntimeError("Require two distinct fully seeded saved boards")
        self.home()
        self.tap("Saved games")
        count = len(expected)
        self.wait("saved-list", lambda t:
                  selected_saved_index(t, count) >= 0)
        found = []
        for idx in range(count):
            tree = self.wait("saved-row-%d" % idx, lambda t:
                             selected_saved_index(t, count) >= 0)
            self.tap_node(saved_rows(tree)[idx])
            selected = self.wait("saved-selected-%d" % idx, lambda t:
                                 selected_saved_index(t, count) == idx)
            primary = self.find(selected, "Continue game")
            if len(primary) != 1 or primary[0].get("clickable") != "true":
                raise RuntimeError("No unique enabled Continue game action")
            self.tap_node(primary[0])
            found.append(normalized_cells(self.board("saved-preview-%d" % idx)))
            self.to_home()
            if idx + 1 < count:
                self.tap("Saved games")
        if len(set(found)) != count or sorted(found) != sorted(expected):
            raise RuntimeError("Saved game previews differ from seeded unique boards")
        return sorted(found)

    def settings(self, set_light=False):
        self.home()
        self.tap("Settings")
        tree = self.wait("settings", lambda t: bool(self.find(t, "Theme:", True)))
        values = self.find(tree, "Theme:", True)
        if len(values) != 1:
            raise RuntimeError("Ambiguous theme setting")
        if set_light and values[0].get("content-desc") == "Theme: Dark":
            self.tap_node(values[0])
            tree = self.wait("light-theme", lambda t: bool(self.find(t, "Theme: Light")))
        value = self.find(tree, "Theme:", True)[0].get("content-desc")
        self.tap("Back: Back")
        self.home()
        return value

    def interrupted(self):
        self.home()
        self.tap("Start new game")
        tree = self.wait("new-game", lambda t: len(parse_cells(t)) == 81 or
                         bool(self.find(t, "New")))
        if self.find(tree, "New"):
            self.tap("New")
        tree = self.board("new-interrupted")
        empty = next(n for _, (v, n) in sorted(parse_cells(tree).items()) if v.startswith("Empty"))
        self.tap_node(empty)
        self.tap("1")
        self.tap("Menu")
        tree = self.wait("leave-choice", lambda t: bool(self.find(t, "Abandon attempt")))
        self.tap("Abandon attempt")
        self.home()
        self.tap("Your journey")
        self.tap("Try again")
        tree = self.wait("journey-interrupted", lambda t:
                         any("Interrupted attempt" in n.get("content-desc", "")
                             for n in t.iter("node")))
        items = sorted(n.get("content-desc") for n in tree.iter("node")
                       if "Interrupted attempt" in n.get("content-desc", ""))
        self.tap("Back")
        self.home()
        return items

    def private_files(self):
        if self.shell("id", "-u").strip() != "0":
            return None
        # On Android SDL_GetPrefPath maps directly to the app's files/ root.
        # Use an absolute path: adb shell sh -c can split a compound command.
        root = "/data/user/0/" + PACKAGE + "/files"
        output = self.shell("find", root, "-type", "f", "-exec",
                            "sha256sum", "{}", "+")
        inventory = parse_hashes(output)
        if root + "/profile.dat" not in inventory:
            raise RuntimeError("No verified private Sudokura profile data")
        if any(not path.startswith(root + "/") for path in inventory):
            raise RuntimeError("Private inventory contains unexpected path")
        return inventory

    def check_apks(self):
        certificates = []
        for path, version, code in ((self.args.old, "1.4.0", "1"),
                                    (self.args.new, "1.4.1", "2")):
            badging = subprocess.check_output([self.args.aapt, "dump", "badging", str(path)],
                                              text=True)
            expected = "package: name='%s' versionCode='%s' versionName='%s'" % (PACKAGE, code, version)
            if expected not in badging:
                raise RuntimeError("Signed test APK package/version mismatch")
            cert = subprocess.check_output([self.args.apksigner, "verify", "--print-certs",
                                            "--verbose", str(path)], text=True)
            certificates.append(signing_cert(cert))
        if certificates[0] != certificates[1] or certificates[0] == PUBLIC_CERT:
            raise RuntimeError("Expected same disposable test signer, never production signer")
        return certificates[0]

    def run(self):
        if not re.fullmatch(r"emulator-\d+", self.serial):
            raise RuntimeError("Only disposable emulator serials are allowed")
        if self.shell("getprop", "ro.kernel.qemu") != "1":
            raise RuntimeError("Refusing a physical Android device")
        if self.shell("getprop", "ro.build.version.sdk") != "30":
            raise RuntimeError("Require the already validated API30 image")
        if "arm64-v8a" not in self.shell("getprop", "ro.product.cpu.abilist").split(","):
            raise RuntimeError("ARM64 translator unavailable")
        if self.shell("getprop", "ro.dalvik.vm.native.bridge") != "libndk_translation.so":
            raise RuntimeError("Unexpected native bridge")
        cert = self.check_apks()
        if package_present(self.shell("pm", "list", "packages", PACKAGE), PACKAGE):
            raise RuntimeError("Refuse emulator with existing Sudokura installation")
        root_attempt = self.adb("root", check=False, timeout=15)
        if "restarting" in root_attempt or "already running as root" in root_attempt:
            self.adb("wait-for-device", timeout=45)
        root_available = self.shell("id", "-u") == "0"
        self.adb("install", str(self.args.old), timeout=100)
        self.start()
        self.home()
        self.tap("Start new game")
        classic_expected = self.seed("classic")
        self.to_home()
        self.tap("Daily Sudoku")
        daily_expected = self.seed("daily")
        self.to_home()
        before_theme = self.settings(set_light=True)
        before_saved = self.saved_previews((classic_expected, daily_expected))
        before_history = self.interrupted()
        self.shell("am", "force-stop", PACKAGE)
        before_files = self.private_files() if root_available else None
        (self.out / "before-private-hashes.json").write_text(json.dumps(before_files, indent=2))
        self.adb("install", "-r", str(self.args.new), timeout=100)
        after_files = self.private_files() if root_available else None
        (self.out / "after-install-private-hashes.json").write_text(json.dumps(after_files, indent=2))
        if before_files is not None and before_files != after_files:
            raise RuntimeError("Private saved data altered by package installation")
        installed = self.shell("dumpsys", "package", PACKAGE)
        if "versionCode=2" not in installed:
            raise RuntimeError("Android package manager did not install code2")
        self.start()
        after_theme = self.settings()
        after_saved = self.saved_previews((classic_expected, daily_expected))
        self.home()
        self.tap("Your journey")
        self.tap("Try again")
        tree = self.wait("journey-after", lambda t:
                         any("Interrupted attempt" in n.get("content-desc", "")
                             for n in t.iter("node")))
        after_history = sorted(n.get("content-desc") for n in tree.iter("node")
                               if "Interrupted attempt" in n.get("content-desc", ""))
        if before_theme != after_theme or before_saved != after_saved or before_history != after_history:
            raise RuntimeError("Saved game, notes, settings or history changed across update")
        receipt = {
            "source_old": OLD_SHA, "source_new": NEW_SHA,
            "package": PACKAGE, "old_code": 1, "new_code": 2,
            "signer": "ephemeral_test_only", "test_signer_sha256": cert,
            "same_signer": "PASS", "adb_install_r": "PASS",
            "classic_daily_note_preview_preservation": "PASS",
            "interrupted_history_preservation": "PASS",
            "settings_preservation": "PASS",
            "private_files_unchanged_on_install": "PASS" if before_files is not None else "NOT_ESTABLISHED",
            "authentic_production_signer_upgrade": "NOT_RUN",
            "physical_arm64_16k": "NOT_RUN",
            "replay_ancestry_and_deletion_receipts": "NOT_RUN"
        }
        (self.out / "upgrade-receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
        print(json.dumps(receipt, sort_keys=True))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--adb", required=True)
    parser.add_argument("--aapt", required=True)
    parser.add_argument("--apksigner", required=True)
    parser.add_argument("--serial", default="emulator-5554")
    parser.add_argument("--old", type=Path, required=True)
    parser.add_argument("--new", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    Upgrade(parser.parse_args()).run()
