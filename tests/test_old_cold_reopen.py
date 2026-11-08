#!/usr/bin/env python3
"""Offline failure/positive fixtures for the disposable old-version baseline."""
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "android" / "scripts"))
from old_cold_reopen import OldBaseline
import old_cold_reopen as baseline

assert baseline.OLD_SHA == "b874e4ea14a8fac6da5f7ac7d540c69bdae38572"
assert baseline.OLD_SHA != baseline.PUBLISHED_SHA

root = "/data/user/0/" + baseline.PACKAGE
files = root + "/files"
inventory = {
    files + "/profile.dat": "a" * 64,
    files + "/profile.lock": "b" * 64,
    files + "/games/first.dat": "c" * 64,
    files + "/games/second.dat": "d" * 64,
}


def fake_snapshot(temp, contents=inventory, override_size=None):
    sample = object.__new__(OldBaseline)
    sample.out = Path(temp)
    sample.stage = lambda label: None
    sample.private_files = lambda: dict(contents)
    sample.private_metadata = lambda hashes: {
        "package_uid": 10123,
        "paths": {path: {"uid": 10123, "gid": 10123,
                          "mode": "600", "selinux": "u:object_r:app_data_file:s0:c1,c2"}
                  for path in (root, files, files + "/games", *hashes)}
    }
    def shell(*args):
        assert args[:3] == ("stat", "-c", "%s")
        return override_size if override_size is not None else "123"
    sample.shell = shell
    return sample


with tempfile.TemporaryDirectory() as temp:
    probe = fake_snapshot(temp)
    result = OldBaseline.capture_private(probe, "A-before-stop")
    assert result["source"] == baseline.OLD_SHA
    assert len(result["files_sha256"]) == 4
    assert len(result["metadata"]["paths"]) == 7
    assert all(row["present"] and row["bytes"] == 123
               for row in result["metadata"]["paths"].values())
    assert json.loads((Path(temp) / "A-before-stop.json").read_text()) == result
    for invalid in (
        {path: sha for path, sha in inventory.items() if not path.endswith("profile.lock")},
        {path: sha for path, sha in inventory.items() if not path.endswith("second.dat")},
        {files + "/profile.dat": "a" * 64,
         files + "/profile.lock": "b" * 64,
         "/tmp/foreign.dat": "c" * 64,
         files + "/games/first.dat": "d" * 64,
         files + "/games/second.dat": "e" * 64},
    ):
        probe = fake_snapshot(temp, contents=invalid)
        try:
            OldBaseline.capture_private(probe, "invalid")
        except RuntimeError:
            pass
        else:
            raise AssertionError("Incomplete/foreign private evidence accepted")
    probe = fake_snapshot(temp, override_size="not-a-length")
    try:
        OldBaseline.capture_private(probe, "invalid-length")
    except RuntimeError:
        pass
    else:
        raise AssertionError("Malformed file length accepted")


with tempfile.TemporaryDirectory() as temp:
    sample = object.__new__(OldBaseline)
    sample.out = Path(temp)
    order = []
    sample.stage = lambda label: order.append(("stage", label))
    sample.shell = lambda *args: order.append(("shell", args))
    sample.capture_private = lambda label: (
        order.append(("capture", label)) or
        {"files_sha256": dict(inventory), "metadata": {"uid": 10123}}
    )
    sample.start = lambda: order.append(("start",))
    sample.settings = lambda: "Theme: Light"
    sample.saved_previews = lambda boards: ["classic", "daily"]
    sample.history = lambda: order.append(("history",)) or ["Interrupted attempt"]
    OldBaseline.checkpoint(sample, "A", (), "Theme: Light", ["classic", "daily"])
    assert not any(item[0] == "history" for item in order)
    assert (Path(temp) / "A-receipt.json").is_file()
    assert order.index(("capture", "A-before-stop")) < order.index(
        ("shell", ("am", "force-stop", baseline.PACKAGE))) < order.index(
        ("capture", "A-after-stop")) < order.index(("start",))
    OldBaseline.checkpoint(sample, "B", (), "Theme: Light", ["classic", "daily"],
                           ["Interrupted attempt"])
    assert any(item[0] == "history" for item in order)
    assert json.loads((Path(temp) / "B-receipt.json").read_text())[
        "interrupted_history"] == "PASS"
    sample.saved_previews = lambda boards: ["duplicate", "duplicate"]
    try:
        OldBaseline.checkpoint(sample, "bad-boards", (), "Theme: Light",
                               ["classic", "daily"])
    except RuntimeError:
        pass
    else:
        raise AssertionError("Wrong saved boards accepted")
    sample.saved_previews = lambda boards: ["classic", "daily"]
    sample.history = lambda: ["Different attempt"]
    try:
        OldBaseline.checkpoint(sample, "bad-history", (), "Theme: Light",
                               ["classic", "daily"], ["Interrupted attempt"])
    except RuntimeError:
        pass
    else:
        raise AssertionError("Wrong Interrupted history accepted")
    sample.capture_private = lambda label: (
        {"files_sha256": dict(inventory), "metadata": {"uid": 10123}}
        if label.endswith("before-stop") else
        {"files_sha256": {}, "metadata": {"uid": 10123}}
    )
    start_before = order.count(("start",))
    try:
        OldBaseline.checkpoint(sample, "bad-force-stop", (), "Theme: Light",
                               ["classic", "daily"])
    except RuntimeError as exc:
        assert "changed during force-stop" in str(exc)
    else:
        raise AssertionError("Force-stop changing private files accepted")
    assert order.count(("start",)) == start_before


with tempfile.TemporaryDirectory() as temp:
    sample = object.__new__(OldBaseline)
    sample.out = Path(temp)
    sample.serial = "emulator-5554"
    sample.args = SimpleNamespace(old=Path("synthetic-original-only.apk"))
    calls = []
    props = {"ro.kernel.qemu": "1", "ro.build.version.sdk": "30",
             "ro.product.cpu.abilist": "x86_64,arm64-v8a",
             "ro.dalvik.vm.native.bridge": "libndk_translation.so"}
    def shell(*args):
        if args[0] == "getprop":
            return props[args[1]]
        if args == ("pm", "list", "packages", baseline.PACKAGE):
            return ""
        if args == ("id", "-u"):
            return "0"
        raise AssertionError("Unexpected real shell call: %r" % (args,))
    sample.shell = shell
    sample.verify_original_apk = lambda: "d" * 64
    sample.adb = lambda *args, **kwargs: calls.append(("adb", args)) or ""
    sample.stage = lambda label: calls.append(("stage", label))
    sample.start = lambda: calls.append(("start",))
    sample.home = lambda: None
    sample.tap = lambda label: calls.append(("tap", label))
    sample.seed = lambda label: label
    sample.to_home = lambda: None
    sample.settings = lambda **kw: "Theme: Light"
    sample.saved_previews = lambda expected: ["classic", "daily"]
    sample.interrupted = lambda: ["Interrupted attempt"]
    def checkpoint(label, boards, theme, saved, history=None):
        calls.append(("checkpoint", label, bool(history)))
    sample.checkpoint = checkpoint
    OldBaseline.run(sample)
    assert [c for c in calls if c[0] == "adb"] == [
        ("adb", ("root",)), ("adb", ("install", "synthetic-original-only.apk"))]
    assert [c for c in calls if c[0] == "checkpoint"] == [
        ("checkpoint", "stage-A-two-saves", False),
        ("checkpoint", "stage-B-interrupted", True)]
    assert (Path(temp) / "baseline-receipt.json").exists()
    props["ro.kernel.qemu"] = "0"
    try:
        OldBaseline.run(sample)
    except RuntimeError as exc:
        assert "physical" in str(exc)
    else:
        raise AssertionError("Physical Android target accepted")

script_source = (Path(__file__).resolve().parents[1] / "android" / "scripts" /
                 "old_cold_reopen.py").read_text()
assert '"-r"' not in script_source
assert '"--new"' not in script_source
assert "source141" not in script_source
print("PASS: old-only two-checkpoint ordering, metadata, error gates and device safety")
