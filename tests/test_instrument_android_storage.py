#!/usr/bin/env python3
"""Offline fail-closed checks for the disposable Android-only instrumentation."""
import importlib.util
import json
from pathlib import Path
import tempfile

script = Path(__file__).resolve().parents[1] / "android" / "scripts" / "instrument_android_storage.py"
spec = importlib.util.spec_from_file_location("instrument_android_storage", script)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

assert len(module.PATCHES) == 3
assert all("src/" in path for path in module.PATCHES)


def prepare_fixture(source):
    old_shas = {}
    for relative, entry in module.PATCHES.items():
        path = source / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        # Non-overlapping anchors simulate the exact contract independently.
        data = ("\n".join(before for before, _ in entry["changes"]) + "\n").encode()
        path.write_bytes(data)
        old_shas[relative] = entry["blob"]
        entry["blob"] = module.blob_sha(data)
    return old_shas


with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp) / "source140"
    old = prepare_fixture(root)
    before = {rel: (root / rel).read_bytes() for rel in module.PATCHES}
    planned = module.prepare(root)
    assert set(planned) == set(module.PATCHES)
    for rel, (_, content, _) in planned.items():
        assert module.TAG.encode() in content
        assert b"__android_log_print" in content
        assert b"/data/user/0/" not in content
        assert b"profile.dat" not in content or rel.endswith("01_runtime.inc")
        assert content != before[rel]
    manifest = Path(tmp) / "receipt.json"
    module.apply(root, manifest)
    receipt = json.loads(manifest.read_text())
    assert receipt["kind"] == "LAB_INSTRUMENTED_NOT_PUBLISHED_APK"
    assert receipt["production_signer"] == "NOT_USED"
    assert receipt["version2_install"] == "NOT_RUN"
    assert len(receipt["traced_paths"]) == 3
    for rel in module.PATCHES:
        assert (root / rel).read_bytes() == planned[rel][1]
    try:
        module.prepare(root)
    except RuntimeError as error:
        assert "Unexpected source blob" in str(error)
    else:
        raise AssertionError("Double instrumentation unexpectedly accepted")
    # Refuse an ambiguous anchor before writing any scratch source file.
    root2 = Path(tmp) / "duplicate"
    prepare_fixture(root2)
    dup_path = root2 / "src/store_io.c"
    text = dup_path.read_text()
    anchor = module.PATCHES["src/store_io.c"]["changes"][0][0]
    dup_path.write_text(text + anchor)
    module.PATCHES["src/store_io.c"]["blob"] = module.blob_sha(dup_path.read_bytes())
    baseline = {rel: (root2 / rel).read_bytes() for rel in module.PATCHES}
    try:
        module.apply(root2, Path(tmp) / "should-not-exist.json")
    except RuntimeError as error:
        assert "Ambiguous or missing patch anchor" in str(error)
    else:
        raise AssertionError("Ambiguous injection source unexpectedly accepted")
    assert all((root2 / rel).read_bytes() == data for rel, data in baseline.items())
    assert not (Path(tmp) / "should-not-exist.json").exists()
    for rel, blob in old.items():
        assert len(blob) == 40

print("PASS: exact-source gated Android trace, metadata sanitization, no double patch and atomic validation")
