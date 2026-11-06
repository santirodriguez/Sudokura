#!/usr/bin/env python3
"""Capture unretouched Android store-review screens on a disposable emulator.

This script never changes production code, signs an APK, or modifies user data.
The x86_64 prototype is a UI-equivalent review build; actual ARM64 release
signature, store-submission readiness and human visual acceptance are separate.
"""
import argparse
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import subprocess
import time
import xml.etree.ElementTree as ET

from PIL import Image, ImageChops
from store_capture_flow import (
    CELL, board_ready, keep_current_ready, replay_confirmation_ready,
    storage_failure,
)

PACKAGE = "org.sudokura.prototype.debug"
ACTIVITY = PACKAGE + "/org.sudokura.prototype.PrototypeActivity"
BOUNDS = re.compile(r"^\[(\d+),(\d+)\]\[(\d+),(\d+)\]$")
SIZE = (1080, 1920)
ROLES = (
    "01-home-dark", "02-classic-dark", "03-daily-dark",
    "04-achieved-dark", "05-try-again-dark", "06-saved-games-light",
)


def solve_sudoku(values):
    """Find one exact solution for the real, accessibility-observed givens."""
    board = list(values)
    if len(board) != 81 or any(not 0 <= x <= 9 for x in board):
        raise ValueError("Invalid observed Sudoku board")
    def candidates(pos):
        r, c = divmod(pos, 9)
        used = set(board[r * 9:(r + 1) * 9])
        used.update(board[c::9])
        br, bc = r // 3 * 3, c // 3 * 3
        used.update(board[(br + i) * 9 + bc + j]
                    for i in range(3) for j in range(3))
        return [x for x in range(1, 10) if x not in used]
    def search():
        slots = [i for i, n in enumerate(board) if n == 0]
        if not slots:
            return True
        pos = min(slots, key=lambda i: len(candidates(i)))
        for n in candidates(pos):
            board[pos] = n
            if search():
                return True
        board[pos] = 0
        return False
    if not search():
        raise RuntimeError("Observed Sudoku has no solution")
    for row in range(9):
        if set(board[row * 9:(row + 1) * 9]) != set(range(1, 10)):
            raise RuntimeError("Invalid solved row")
    return board


class Capture:
    def __init__(self, args):
        self.args = args
        self.root = args.output.resolve()
        self.raw = self.root / "native"
        self.rgb = self.root / "review"
        self.evidence = self.root / "evidence"
        for directory in (self.raw, self.rgb, self.evidence):
            directory.mkdir(parents=True, exist_ok=True)
        self.actions = []
        self.images = []
        self.latest = None
        self.started = time.monotonic()

    def adb(self, *command, binary=False, timeout=60, check=True):
        p = subprocess.run([self.args.adb, "-s", self.args.serial, *command],
                           capture_output=True, text=not binary, timeout=timeout)
        if check and p.returncode:
            raise RuntimeError("adb " + " ".join(command) + ": " +
                               str(p.stderr)[-900:])
        return p.stdout

    def shell(self, *cmd, timeout=60):
        return self.adb("shell", *cmd, timeout=timeout).strip()

    def evidence_png(self, name):
        data = self.adb("exec-out", "screencap", "-p", binary=True)
        if not data.startswith(b"\x89PNG\r\n\x1a\n"):
            raise RuntimeError("Android screencap did not produce PNG")
        (self.raw / (name + ".png")).write_bytes(data)
        return data

    def dump(self, name):
        remote = "/sdcard/sudokura-store-review.xml"
        failures = []
        for attempt in range(3):
            try:
                self.shell("rm", "-f", remote)
                self.shell("uiautomator", "dump", remote, timeout=45)
                xml = self.adb("exec-out", "cat", remote)
                tree = ET.fromstring(xml)
                self.latest = tree
                (self.evidence / (name + ".xml")).write_text(xml, encoding="utf-8")
                return tree
            except (RuntimeError, ET.ParseError, subprocess.TimeoutExpired) as exc:
                failures.append(str(exc))
                (self.evidence / (name + "-dump-error.txt")).write_text(
                    "\n".join(failures) + "\n", encoding="utf-8")
                booted = self.shell("getprop", "sys.boot_completed")
                window = self.shell("service", "check", "window")
                if booted != "1" or not window.strip().lower().endswith(": found"):
                    raise RuntimeError(
                        "Emulator system services failed during capture: " +
                        repr((booted, window))) from exc
                if attempt == 2:
                    raise RuntimeError(
                        "Three failed uiautomator dumps for " + name) from exc
                time.sleep(2)
        raise AssertionError("Unreachable dump retry exit")

    @staticmethod
    def nodes(tree):
        return list(tree.iter("node"))

    @staticmethod
    def descriptions(tree):
        return [n.get("content-desc", "") for n in tree.iter("node")]

    def wait(self, name, predicate, seconds=100):
        deadline = time.monotonic() + seconds
        last = None
        attempt = 0
        while time.monotonic() < deadline:
            attempt += 1
            tree = self.dump(name)
            failure = storage_failure(tree)
            if failure:
                self.actions.append({"action": "storage recovery blocked capture",
                                     "stage": name})
                self.evidence_png(name + "-storage-error")
                self.write_receipt()
                raise RuntimeError("Storage save failed during " + name +
                                   ": " + failure)
            layout = [(n.get("class"), n.get("bounds"), n.get("enabled"))
                      for n in self.nodes(tree)]
            if predicate(tree):
                if layout == last:
                    return tree
                last = layout
            else:
                last = None
            time.sleep(1.1)
        self.evidence_png(name + "-failed")
        raise RuntimeError("Screen did not stabilize: " + name +
                           " / labels: " + repr(self.descriptions(tree)[:22]))

    def find(self, tree, label=None, pattern=None, last=False):
        matches = []
        for n in self.nodes(tree):
            desc = n.get("content-desc", "")
            if ((label is not None and desc == label) or
                    (pattern is not None and re.search(pattern, desc))):
                if n.get("enabled") == "true" and n.get("bounds"):
                    matches.append(n)
        if not matches:
            raise RuntimeError("Action missing: " + str(label or pattern) +
                               " / labels: " + repr(self.descriptions(tree)[:30]))
        return matches[-1] if last else matches[0]

    def tap(self, node, reason):
        m = BOUNDS.fullmatch(node.get("bounds", ""))
        if not m:
            raise RuntimeError("Unusable bounds: " + reason)
        a, b, c, d = map(int, m.groups())
        x, y = (a+c)//2, (b+d)//2
        if not (0 <= x < SIZE[0] and 0 <= y < SIZE[1]):
            raise RuntimeError("Action outside actual phone display: " + reason)
        self.shell("input", "tap", str(x), str(y))
        self.actions.append({"action": reason, "x": x, "y": y})
        time.sleep(0.08)

    def press(self, tree, label=None, pattern=None, last=False):
        node = self.find(tree, label=label, pattern=pattern, last=last)
        self.tap(node, label or pattern)
        # The next wait() verifies the new semantic scene. An immediate
        # throwaway dump doubles the UIAutomation load for no benefit.

    def capture(self, role, tree, expected):
        if role not in ROLES or role in [x["role"] for x in self.images]:
            raise RuntimeError("Unexpected/duplicate screenshot role")
        if not expected(tree):
            raise RuntimeError("Scene is not authentic for " + role)
        original = self.evidence_png(role)
        with Image.open(io.BytesIO(original)) as source:
            source.load()
            if source.size != SIZE:
                raise RuntimeError("Wrong native capture dimensions: " + str(source.size))
            if "A" in source.getbands() and source.getchannel("A").getextrema() != (255,255):
                raise RuntimeError("Screenshot contains nonopaque alpha")
            visible = source.convert("RGB")
        output = self.rgb / (role + ".png")
        visible.save(output, format="PNG")
        with Image.open(output) as reopened:
            if reopened.size != SIZE or reopened.mode != "RGB":
                raise RuntimeError("PNG RGB export mismatch")
            if ImageChops.difference(reopened, visible).getbbox():
                raise RuntimeError("Visible pixels changed in lossless conversion")
        self.images.append({
            "role": role, "native": str((self.raw / (role + ".png")).relative_to(self.root)),
            "rgb": str(output.relative_to(self.root)),
            "native_sha256": hashlib.sha256(original).hexdigest(),
            "rgb_sha256": hashlib.sha256(output.read_bytes()).hexdigest(),
            "size": list(SIZE), "mode": "RGB",
            "labels": self.descriptions(tree)[:28],
        })
        self.write_receipt()

    def clean_emulator_status_bar(self):
        # Android SystemUI demo mode changes the real device status bar,
        # never the captured image bytes. Hide transient notification icons.
        self.shell("settings", "put", "global", "sysui_demo_allowed", "1")
        action = ("am", "broadcast", "-a", "com.android.systemui.demo",
                  "-e", "command")
        self.shell(*action, "enter")
        self.shell(*action, "clock", "-e", "hhmm", "0941")
        self.shell(*action, "notifications", "-e", "visible", "false")
        self.actions.append({"action": "clean emulator status bar",
                             "mode": "Android SystemUI demo",
                             "clock": "09:41", "notification_icons": "hidden"})

    def write_receipt(self):
        (self.root / "capture-manifest.json").write_text(json.dumps({
            "source_sha": self.args.source,
            "apk_sha256": hashlib.sha256(self.args.apk.read_bytes()).hexdigest(),
            "android_variant": "prototypeDebug/x86_64",
            "production_arm64_release_acceptance": "NOT_ESTABLISHED",
            "device": self.args.serial, "resolution": list(SIZE),
            "language": "en-US", "screens": self.images,
            "actions": self.actions,
            "store_approval": "PENDING_VISUAL_REVIEW",
        }, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    @staticmethod
    def cells(tree):
        found = {}
        for node in tree.iter("node"):
            label = node.get("content-desc", "")
            match = CELL.match(label)
            if match:
                r, c, kind, digit = match.groups()
                found[(int(r)-1)*9 + int(c)-1] = {
                    "node": node, "kind": kind,
                    "value": int(digit or 0), "label": label
                }
        return found

    def board(self, name):
        t = self.wait(name, board_ready)
        grid = self.cells(t)
        given = [grid[i]["value"] for i in range(81)]
        if sum(bool(v) for v in given) < 17:
            raise RuntimeError("Insufficient real puzzle givens")
        return t, grid, solve_sudoku(given)

    def fill(self, tree, grid, solution, indices):
        digits = {}
        for n in self.nodes(tree):
            label = n.get("content-desc", "")
            if n.get("class") == "android.widget.Button" and re.fullmatch(r"[1-9]", label):
                digits[int(label)] = n
        if set(digits) != set(range(1, 10)):
            raise RuntimeError("Missing real numeric controls: " + repr(sorted(digits)))
        for i in indices:
            self.tap(grid[i]["node"], "select cell " + str(i))
            self.tap(digits[solution[i]], "enter correct digit " + str(solution[i]))
        self.actions.append({"action": "filled real observed cells", "count": len(indices)})

    def ensure_theme(self, desired):
        home = self.wait("before-theme-"+desired, lambda x: "Start new game" in self.descriptions(x))
        self.press(home, label="Settings")
        settings = self.wait("settings-"+desired, lambda x:
                             any(v.startswith("Theme: ") for v in self.descriptions(x)))
        theme = next(v for v in self.descriptions(settings) if v.startswith("Theme: "))
        if theme != "Theme: " + desired:
            self.press(settings, label=theme)
            settings = self.wait("settings-changed-"+desired, lambda x:
                                 "Theme: " + desired in self.descriptions(x))
        self.press(settings, pattern=r"^Back: ")
        return self.wait("home-theme-"+desired, lambda x:
                         "Start new game" in self.descriptions(x))

    def run(self):
        if not re.fullmatch(r"emulator-[0-9]+", self.args.serial):
            raise ValueError("Only a disposable emulator may be used")
        if not re.fullmatch(r"[0-9a-f]{40}", self.args.source):
            raise ValueError("Invalid exact source commit")
        if self.shell("getprop", "ro.kernel.qemu") != "1":
            raise RuntimeError("Refusing a physical device")
        if self.shell("getprop", "ro.product.cpu.abi") != "x86_64":
            raise RuntimeError("Unexpected emulator ABI")
        # adb 'device' precedes framework readiness. WindowManager may not
        # exist for the first seconds after the emulator comes online.
        deadline = time.monotonic() + 180
        while time.monotonic() < deadline:
            booted = self.shell("getprop", "sys.boot_completed") == "1"
            window = self.shell("service", "check", "window")
            if booted and window.strip().lower().endswith(": found"):
                break
            time.sleep(2)
        else:
            raise RuntimeError("Android boot/window service not ready")
        # Boot-completed can precede the initial system UI/keyguard transition.
        # Allow the disposable emulator to settle before launching the app.
        time.sleep(12)
        self.shell("wm", "size", "1080x1920")
        self.shell("wm", "density", "420")
        self.shell("settings", "put", "global", "device_provisioned", "1")
        self.shell("settings", "put", "secure", "user_setup_complete", "1")
        self.shell("input", "keyevent", "KEYCODE_WAKEUP")
        self.shell("wm", "dismiss-keyguard")
        self.adb("install", "-r", str(self.args.apk))
        self.shell("pm", "clear", PACKAGE)
        self.adb("logcat", "-c")
        self.shell("am", "start", "-W", "-n", ACTIVITY)
        self.clean_emulator_status_bar()
        home = self.ensure_theme("Dark")
        self.capture(ROLES[0], home, lambda x: "Start new game" in self.descriptions(x)
                     and "Your journey" in self.descriptions(x))

        # A live easy Classic attempt: normal entries and authentic pencil notes.
        home = self.press(home, label="Classic")
        t = self.wait("classic-home", lambda x: "Start new game" in self.descriptions(x))
        self.press(t, label="Start new game")
        classic, grid, answer = self.board("classic-board")
        blanks = [i for i in range(81) if grid[i]["kind"] == "Empty"]
        if len(blanks) < 10:
            raise RuntimeError("No representative editable Classic puzzle")
        self.fill(classic, grid, answer, blanks[:4])
        now = self.wait("classic-four-values", lambda x:
                        len(self.cells(x)) == 81 and
                        sum(v["kind"] == "Entered" for v in self.cells(x).values()) >= 4)
        self.press(now, label="Notes")
        notes = self.wait("classic-notes", lambda x:
                          any(n.get("content-desc") == "Notes" and n.get("selected") == "true"
                              for n in self.nodes(x)))
        self.tap(self.cells(notes)[blanks[4]]["node"], "select note cell")
        digit = next(n for n in self.nodes(notes)
                     if n.get("content-desc") == str(answer[blanks[4]]) and
                     n.get("class") == "android.widget.Button")
        self.tap(digit, "write real note")
        notes = self.wait("classic-noted", lambda x:
                          "Notes" in self.cells(x).get(blanks[4], {}).get("label", ""))
        self.press(notes, label="Notes")
        classic = self.wait("classic-shot", lambda x:
                            len(self.cells(x)) == 81 and
                            sum(v["kind"] == "Entered" for v in self.cells(x).values()) >= 4)
        self.capture(ROLES[1], classic, lambda x: len(self.cells(x)) == 81)

        # Finish this same real puzzle via its accessible board; a completed
        # record in Achieved is earned by genuine UI interactions.
        grid = self.cells(classic)
        self.fill(classic, grid, answer, blanks[4:])
        win = self.wait("classic-win", lambda x: "Back to Home" in self.descriptions(x) and
                        "You win" in self.descriptions(x), seconds=150)
        self.press(win, label="Back to Home")
        home = self.wait("home-after-win", lambda x: "Daily Sudoku" in self.descriptions(x))

        # Today's actual Daily, with its own visible identity, notes and digits.
        self.press(home, label="Daily Sudoku")
        daily, grid, answer = self.board("daily-board")
        blanks = [i for i in range(81) if grid[i]["kind"] == "Empty"]
        self.fill(daily, grid, answer, blanks[:4])
        daily = self.wait("daily-shot", lambda x:
                          len(self.cells(x)) == 81 and
                          sum(v["kind"] == "Entered" for v in self.cells(x).values()) >= 4)
        self.capture(ROLES[2], daily, lambda x: len(self.cells(x)) == 81)
        self.press(daily, label="Menu")
        leaving = self.wait("daily-leave-modal", lambda x:
                            "Abandon attempt" in self.descriptions(x))
        self.press(leaving, label="Abandon attempt")
        home = self.wait("home-after-daily-abandon", lambda x:
                         "Your journey" in self.descriptions(x))

        # Achieved shows the actual completed Classic result.
        self.press(home, label="Your journey")
        browser = self.wait("journey-tabs", lambda x:
                            "Achieved" in self.descriptions(x) and
                            "Try again" in self.descriptions(x))
        self.press(browser, label="Achieved")
        achieved = self.wait("achieved-record", lambda x:
                             any("You win" in y for y in self.descriptions(x)) and
                             "Achieved" in self.descriptions(x))
        self.capture(ROLES[3], achieved, lambda x:
                     any("You win" in y for y in self.descriptions(x)))

        # Retry the real interrupted Daily attempt, not a mockup.
        self.press(achieved, label="Try again")
        retry = self.wait("try-again-list", lambda x:
                          any("Interrupted attempt" in y for y in self.descriptions(x)))
        row = self.find(retry, pattern=r"Interrupted attempt")
        self.tap(row, "choose interrupted original")
        retry = self.wait("try-again-selected", lambda x:
                          "Try again" in self.descriptions(x))
        self.press(retry, label="Try again", last=True)
        confirmation = self.wait("replay-confirmation", replay_confirmation_ready)
        self.press(confirmation, label="Replay", last=True)
        # An already finished attempt activates Replay directly. Only a
        # genuinely active, unsaved departing attempt needs the keep gate.
        activation = self.wait("replay-activation",
                               lambda x: board_ready(x) or keep_current_ready(x))
        if keep_current_ready(activation):
            self.press(activation, label="Save and leave")
        replay, grid, answer = self.board("replayed-real-puzzle")
        blanks = [i for i in range(81) if grid[i]["kind"] == "Empty"]
        self.fill(replay, grid, answer, blanks[:3])
        replay = self.wait("replay-with-progress", lambda x:
                           len(self.cells(x)) == 81 and
                           sum(v["kind"] == "Entered" for v in self.cells(x).values()) >= 3)
        self.capture(ROLES[4], replay, lambda x: len(self.cells(x)) == 81)

        # Save the current replay as a real Saved Games entry.
        self.press(replay, label="Menu")
        leave = self.wait("replay-leave-modal", lambda x:
                          "Save and leave" in self.descriptions(x))
        self.press(leave, label="Save and leave")
        home = self.wait("home-after-save", lambda x:
                         "Start new game" in self.descriptions(x))
        home = self.ensure_theme("Light")
        self.press(home, label="Saved games")
        saved = self.wait("saved-games-light", lambda x:
                          "Your saved games" in self.descriptions(x) and
                          any("In progress" in y for y in self.descriptions(x)))
        self.capture(ROLES[5], saved, lambda x:
                     "Your saved games" in self.descriptions(x))
        if [i["role"] for i in self.images] != list(ROLES):
            raise RuntimeError("Missing one or more ordered store scenes")
        self.write_receipt()
        print("PASS six original Android screens and pixel-equivalent RGB PNGs")


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--adb", required=True)
    p.add_argument("--apk", type=Path, required=True)
    p.add_argument("--source", required=True)
    p.add_argument("--serial", default="emulator-5554")
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    capture = Capture(args)
    try:
        capture.run()
    except Exception:
        try:
            capture.evidence_png("capture-exception")
            capture.write_receipt()
        finally:
            raise
    finally:
        try:
            capture.shell("pm", "clear", PACKAGE, timeout=30)
        except Exception:
            pass


if __name__ == "__main__":
    main()
