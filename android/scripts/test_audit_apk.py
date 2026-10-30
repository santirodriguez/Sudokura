#!/usr/bin/env python3
"""Offline audit regressions; synthetic ZIPs are not installable APK evidence."""
import contextlib
import io
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest import mock
import zipfile

import audit_apk


def header(major=1, minor=4, patch=0):
    return (f"#define SUDOKURA_VERSION_MAJOR {major}\n"
            f"#define SUDOKURA_VERSION_MINOR {minor}\n"
            f"#define SUDOKURA_VERSION_PATCH {patch}\n")


def badging(version="1.4.0-prototype", code=10400):
    return ("package: name='org.sudokura.prototype.debug' "
            f"versionCode='{code}' versionName='{version}'\n"
            "sdkVersion:'28'\ntargetSdkVersion:'36'\n"
            "native-code: 'arm64-v8a'\n"
            "application: label='Sudokura' icon='res/drawable/icon.xml'\n")


MANIFEST = ("android:icon(0x01010002)=@0x7f010000\n"
            "android:allowBackup(0x01010280)=(type 0x12)0x0\n"
            "android:fullBackupContent(0x010104eb)=@0x7f020000\n"
            "android:dataExtractionRules(0x0101064e)=@0x7f020001\n")


class IdentityTests(unittest.TestCase):
    def test_versions_derive_from_source(self):
        for minor, patch, code in ((3, 9, 10309), (4, 0, 10400), (5, 12, 10512)):
            identity = audit_apk.expected_identity(header(1, minor, patch))
            self.assertEqual(identity["version_code"], code)
            self.assertEqual(identity["version_name"], f"1.{minor}.{patch}-prototype")
            audit_apk.audit_manifest(
                badging(identity["version_name"], code), MANIFEST, identity)

    def test_aapt_line_padding_is_not_a_mismatch(self):
        padded = "\n".join(line + "  " for line in badging().splitlines()) + "\n"
        audit_apk.audit_manifest(padded, MANIFEST, audit_apk.expected_identity(header()))

    def test_missing_duplicate_invalid_and_overflow_fail(self):
        for text in ("", header() + "#define SUDOKURA_VERSION_MINOR 4\n",
                     header(patch=-1), header(patch="0x1"), header(minor=100),
                     header(patch=100), header(0, 0, 0), header(214749, 0, 0)):
            with self.subTest(text=text), self.assertRaises(ValueError):
                audit_apk.expected_identity(text)

    def test_unsupported_variant_fails(self):
        with self.assertRaises(ValueError):
            audit_apk.expected_identity(header(), "release")

    def test_source_header_has_expected_candidate_version(self):
        identity = audit_apk.expected_identity(
            (audit_apk.ROOT / "src/version.h").read_text(encoding="utf-8"))
        self.assertGreaterEqual(identity["version_code"], 10400)
        self.assertTrue(identity["version_name"].endswith("-prototype"))

    def test_stale_version_and_wrong_identity_fail(self):
        for text in (badging("1.3.9-prototype", 10309), badging(code=10309),
                     badging("1.4.0"), badging().replace("prototype.debug", "production")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                audit_apk.audit_manifest(text, MANIFEST, audit_apk.expected_identity(header()))

    def test_package_must_be_unique_and_not_an_incidental_match(self):
        for text in (badging().replace("package:", "activity:"),
                     badging() + badging().splitlines()[0] + "\n",
                     badging().replace("versionCode='10400'", "versionCode='10400' versionCode='10400'")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                audit_apk.audit_manifest(text, MANIFEST, audit_apk.expected_identity(header()))

    def test_permissions_sdk_and_abi_restrictions_remain(self):
        for text in (badging() + "uses-permission: name='android.permission.INTERNET'\n",
                     badging().replace("sdkVersion:'28'", "sdkVersion:'27'"),
                     badging().replace("targetSdkVersion:'36'", "targetSdkVersion:'35'"),
                     badging().replace("'arm64-v8a'", "'arm64-v8a' 'x86_64'")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                audit_apk.audit_manifest(text, MANIFEST, audit_apk.expected_identity(header()))

    def test_backup_and_icon_restrictions_remain(self):
        for text in (MANIFEST.replace("android:icon(", "missing:icon("),
                     MANIFEST.replace("(type 0x12)0x0", "(type 0x12)0xffffffff"),
                     MANIFEST.replace("android:fullBackupContent(", "missing:backup("),
                     MANIFEST.replace("android:dataExtractionRules(", "missing:rules(")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                audit_apk.audit_manifest(badging(), text, audit_apk.expected_identity(header()))
        with self.assertRaises(ValueError):
            audit_apk.audit_manifest(badging().replace("icon='res/drawable/icon.xml'", "icon=''"),
                                     MANIFEST, audit_apk.expected_identity(header()))


class ArchiveTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.audio = self.root / "assets/audio"
        self.audio.mkdir(parents=True)
        (self.root / "src").mkdir()
        (self.root / "src/version.h").write_text(header(), encoding="utf-8")
        self.contents = {path: b"synthetic fixture" for path in audit_apk.EXPECTED_ASSETS}
        for name in audit_apk.AUDIO_NAMES:
            (self.audio / name).write_bytes(self.contents[f"assets/audio/{name}"])
        for name in audit_apk.EXPECTED_NATIVE:
            self.contents[f"lib/arm64-v8a/{name}"] = b"synthetic library"
        self.icon = "res/drawable-nodpi-v4/sudokura_icon_source.png"
        self.contents[self.icon] = b"\x89PNG\r\n\x1a\n" + b"\0" * 8 + struct.pack(">II", 512, 512)
        self.apk = self.root / "synthetic.zip"

    def write_archive(self):
        with zipfile.ZipFile(self.apk, "w") as archive:
            for name, data in self.contents.items():
                archive.writestr(name, data)

    def audit(self):
        self.write_archive()
        return audit_apk.audit_archive(self.apk, self.audio)

    def test_required_structure(self):
        self.assertEqual(len(self.audit()), 4)

    def test_missing_or_empty_notice(self):
        path = "assets/licenses/FreeType-FTL.txt"
        del self.contents[path]
        with self.assertRaises(ValueError):
            self.audit()
        self.contents[path] = b""
        with self.assertRaises(ValueError):
            self.audit()

    def test_missing_native(self):
        del self.contents["lib/arm64-v8a/libmain.so"]
        with self.assertRaises(ValueError):
            self.audit()

    def test_extra_abi(self):
        self.contents["lib/x86_64/libmain.so"] = b"synthetic"
        with self.assertRaises(ValueError):
            self.audit()

    def test_audio_integrity(self):
        self.contents["assets/audio/music-main.ogg"] = b"changed"
        with self.assertRaises(ValueError):
            self.audit()

    def test_icon_integrity(self):
        for data in (b"bad png", self.contents[self.icon][:16] + struct.pack(">II", 256, 512)):
            self.contents[self.icon] = data
            with self.assertRaises(ValueError):
                self.audit()
        del self.contents[self.icon]
        with self.assertRaises(ValueError):
            self.audit()

    def test_main_receipt_never_claims_device_acceptance(self):
        self.write_archive()
        responses = [subprocess.CompletedProcess([], 0, badging(), ""),
                     subprocess.CompletedProcess([], 0, MANIFEST, "")]
        output = io.StringIO()
        with mock.patch.object(audit_apk, "ROOT", self.root), \
             mock.patch.object(audit_apk.subprocess, "run", side_effect=responses) as run, \
             contextlib.redirect_stdout(output):
            audit_apk.main(["fake-aapt", str(self.apk), "--variant", "debug"])
        receipt = json.loads(output.getvalue())
        self.assertEqual(receipt["device_test"], "NOT_RUN")
        self.assertEqual(receipt["version_code"], 10400)
        self.assertEqual(receipt["build_variant"], "debug")
        self.assertFalse(receipt["android_backup"])
        self.assertEqual(run.call_count, 2)
        self.assertEqual(run.call_args_list[0].args[0][1:3], ["dump", "badging"])

    def test_aapt_failure_is_fatal(self):
        self.write_archive()
        with mock.patch.object(audit_apk, "ROOT", self.root), \
             mock.patch.object(audit_apk.subprocess, "run", side_effect=subprocess.CalledProcessError(1, "fake-aapt")), \
             self.assertRaises(SystemExit):
            audit_apk.main(["fake-aapt", str(self.apk)])


if __name__ == "__main__":
    unittest.main()
