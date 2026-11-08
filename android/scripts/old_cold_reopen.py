#!/usr/bin/env python3
"""Two-stage cold reopen of a fixed, nonpublished v1.4.0 test APK."""
import argparse
import json
from pathlib import Path
import re
import subprocess

from ephemeral_upgrade import (
    OLD_SHA as PUBLISHED_SHA, PACKAGE, PUBLIC_CERT, Upgrade, package_present,
    signing_cert, parse_cells
)


# Exact fixed-source v1.4.0 test candidate, never the public 1.4.0 APK.
OLD_SHA = "3077dd6f0b803a523f7ab41d6203dfce9cfde978"

KEEP_TITLE = "Keep your current game?"
KEEP_BODY = ("Loading will replace the current unsaved game. "
             "Save it in Games, discard its progress, or cancel.")


def guarded_save_and_leave(tree):
    """Return only the safe button in the exact, known replacement dialog."""
    nodes = [node for node in tree.iter("node") if node.get("content-desc")]
    labels = [node.get("content-desc") for node in nodes]
    if KEEP_TITLE not in labels:
        return None
    expected = {
        KEEP_TITLE: "android.widget.TextView",
        KEEP_BODY: "android.widget.TextView",
        "Cancel": "android.widget.Button",
        "Save and leave": "android.widget.Button",
        "Abandon attempt": "android.widget.Button",
    }
    if len(labels) != len(expected) or set(labels) != set(expected):
        raise RuntimeError("Unexpected saved-game replacement dialog")
    for node in nodes:
        label = node.get("content-desc")
        button = expected[label] == "android.widget.Button"
        if (node.get("package") != PACKAGE or
                node.get("class") != expected[label] or
                node.get("enabled") != "true" or
                node.get("clickable") != ("true" if button else "false")):
            raise RuntimeError("Unsafe saved-game replacement control")
    return next(node for node in nodes
                if node.get("content-desc") == "Save and leave")


class OldBaseline(Upgrade):
    """Never installs a new version or modifies the product's storage directly."""


    def board(self, label):
        # Only the two known saved-game previews may resolve this UI guard.
        if not re.fullmatch(r"saved-preview-[01]", label):
            return super().board(label)
        response = self.wait(label + "-or-keep", lambda t:
                             len(parse_cells(t)) == 81 or
                             any(n.get("content-desc") == KEEP_TITLE
                                 for n in t.iter("node")) or
                             bool(self.find(t, "Unsaved progress")) or
                             bool(self.find(t, "Storage error")))
        if self.find(response, "Storage error"):
            raise RuntimeError("Storage error during saved-game selection")
        if self.find(response, "Unsaved progress"):
            raise RuntimeError("Unsaved progress while opening saved game")
        if guarded_save_and_leave(response) is not None:
            # Every board was explicitly saved. A Keep prompt here is a
            # regression; never auto-choose Save, Abandon or Discard.
            raise RuntimeError("Unexpected Save-and-leave gate during saved-game verification")
        if len(parse_cells(response)) != 81:
            raise RuntimeError("Unexpected saved-game preview state")
        return super().board(label)

    def verify_original_apk(self):
        badging = subprocess.check_output(
            [self.args.aapt, "dump", "badging", str(self.args.old)], text=True)
        expected = "package: name='%s' versionCode='1' versionName='1.4.0'" % PACKAGE
        if expected not in badging:
            raise RuntimeError("Wrong original APK package/version")
        output = subprocess.check_output(
            [self.args.apksigner, "verify", "--print-certs", "--verbose",
             str(self.args.old)], text=True)
        fingerprint = signing_cert(output)
        if fingerprint == PUBLIC_CERT:
            raise RuntimeError("Production certificate forbidden in test-only baseline")
        return fingerprint

    def capture_private(self, label):
        """Write only paths, hashes, ownership/labels and byte lengths."""
        self.stage(label)
        inventory = self.private_files()
        data_root = "/data/user/0/" + PACKAGE
        root = data_root + "/files/"
        if any(not path.startswith(root) for path in inventory or {}):
            raise RuntimeError("Non-private path in original evidence inventory")
        required = {root + "profile.dat", root + "profile.lock"}
        games = [path for path in inventory or {}
                 if path.startswith(root + "games/") and path.endswith(".dat")]
        if not inventory or not required.issubset(inventory) or len(games) < 2:
            raise RuntimeError("Missing required old-version profile, lock or games")
        metadata = self.private_metadata(inventory)
        if not metadata or not metadata.get("paths"):
            raise RuntimeError("Missing private file metadata")
        for path, entry in metadata["paths"].items():
            if path not in (data_root, root[:-1], root + "games") and path not in inventory:
                raise RuntimeError("Unexpected private metadata path")
            result = self.shell("stat", "-c", "%s", path).strip()
            if not re.fullmatch(r"\d+", result):
                raise RuntimeError("Missing private file length")
            entry["bytes"] = int(result)
            entry["present"] = True
        snapshot = {
            "source": OLD_SHA, "version": "1.4.0", "code": 1,
            "build_kind": "FIXED_V140_TEST_CANDIDATE",
            "package": PACKAGE, "stage": label,
            "files_sha256": inventory, "metadata": metadata
        }
        (self.out / (label + ".json")).write_text(
            json.dumps(snapshot, sort_keys=True, indent=2) + "\n")
        return snapshot

    def checkpoint(self, label, expected_boards, theme, saved, history=None):
        before = self.capture_private(label + "-before-stop")
        self.shell("am", "force-stop", PACKAGE)
        after_stop = self.capture_private(label + "-after-stop")
        if (before["files_sha256"] != after_stop["files_sha256"] or
                before["metadata"] != after_stop["metadata"]):
            raise RuntimeError("Fixed candidate private files changed during force-stop")
        self.stage(label + "-reopen")
        self.start()
        cold_theme = self.settings()
        cold_saved = self.saved_previews(expected_boards)
        cold_history = self.history() if history is not None else None
        if (cold_theme != theme or cold_saved != saved or
                (history is not None and cold_history != history)):
            raise RuntimeError("Fixed candidate cold reopen did not retain " + label)
        self.stage(label + "-verified")
        (self.out / (label + "-receipt.json")).write_text(
            json.dumps({"source": OLD_SHA, "version": "1.4.0",
                        "build_kind": "FIXED_V140_TEST_CANDIDATE",
                        "stage": label, "cold_reopen": "PASS",
                        "two_saved_boards_and_notes": "PASS",
                        "light_theme": "PASS",
                        "interrupted_history": "PASS" if history is not None else "NOT_RUN"},
                       indent=2) + "\n")

    def run(self):
        if not re.fullmatch(r"emulator-\d+", self.serial):
            raise RuntimeError("Only disposable emulator serials are allowed")
        if self.shell("getprop", "ro.kernel.qemu") != "1":
            raise RuntimeError("Refusing a physical Android device")
        if self.shell("getprop", "ro.build.version.sdk") != "30":
            raise RuntimeError("Require Android 11 / API30")
        if "arm64-v8a" not in self.shell("getprop", "ro.product.cpu.abilist").split(","):
            raise RuntimeError("ARM64 translator unavailable")
        if self.shell("getprop", "ro.dalvik.vm.native.bridge") != "libndk_translation.so":
            raise RuntimeError("Unexpected native bridge")
        fingerprint = self.verify_original_apk()
        if package_present(self.shell("pm", "list", "packages", PACKAGE), PACKAGE):
            raise RuntimeError("Refusing an emulator with an existing installation")
        root_result = self.adb("root", check=False, timeout=15)
        if "restarting" in root_result or "already running as root" in root_result:
            self.adb("wait-for-device", timeout=45)
        if self.shell("id", "-u").strip() != "0":
            raise RuntimeError("Require rooted disposable emulator for diagnostics")
        self.stage("install_fixed_candidate")
        self.adb("install", str(self.args.old), timeout=100)
        self.start()
        self.home()
        self.tap("Start new game")
        classic = self.seed("classic")
        self.to_home()
        self.tap("Daily Sudoku")
        daily = self.seed("daily")
        self.to_home()
        theme = self.settings(set_light=True)
        if theme != "Theme: Light":
            raise RuntimeError("Expected expected Light setting")
        saved = self.saved_previews((classic, daily))
        self.checkpoint("stage-A-two-saves", (classic, daily), theme, saved)
        history = self.interrupted()
        if not history or not any("Interrupted attempt" in row for row in history):
            raise RuntimeError("Missing original Interrupted history")
        self.checkpoint("stage-B-interrupted", (classic, daily), theme, saved, history)
        receipt = {
            "source": OLD_SHA, "version": "1.4.0", "code": 1,
            "build_kind": "FIXED_V140_TEST_CANDIDATE",
            "signer": "disposable_test_only", "test_signer_sha256": fingerprint,
            "stage_A_saved_cold_reopen": "PASS",
            "stage_B_interrupted_cold_reopen": "PASS",
            "v141_install": "NOT_RUN", "upgrade_preservation": "NOT_ESTABLISHED",
            "real_device_and_production_signer": "NOT_RUN"
        }
        self.stage("fixed_candidate_baseline_verified")
        (self.out / "baseline-receipt.json").write_text(
            json.dumps(receipt, indent=2) + "\n")
        print(json.dumps(receipt, sort_keys=True))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--adb", required=True)
    parser.add_argument("--aapt", required=True)
    parser.add_argument("--apksigner", required=True)
    parser.add_argument("--serial", default="emulator-5554")
    parser.add_argument("--old", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    OldBaseline(parser.parse_args()).run()
