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


def header(major=1, minor=4, patch=0, code=1):
    return (f"#define SUDOKURA_VERSION_MAJOR {major}\n"
            f"#define SUDOKURA_VERSION_MINOR {minor}\n"
            f"#define SUDOKURA_VERSION_PATCH {patch}\n"
            f"#define SUDOKURA_VERSION_ANDROID_CODE {code}\n")


def badging(version="1.4.0-prototype", code=1):
    return ("package: name='org.sudokura.prototype.debug' "
            f"versionCode='{code}' versionName='{version}'\n"
            "sdkVersion:'28'\ntargetSdkVersion:'36'\n"
            "native-code: 'arm64-v8a'\n"
            "application: label='Sudokura' icon='res/drawable/icon.xml'\n")


MANIFEST = ("android:icon(0x01010002)=@0x7f010000\n"
            "android:allowBackup(0x01010280)=(type 0x12)0x0\n"
            "android:fullBackupContent(0x010104eb)=@0x7f020000\n"
            "android:dataExtractionRules(0x0101064e)=@0x7f020001\n")


RESOURCES = """resource 0x7f020000 org.example:xml/backup_rules:
  (string8) "res/xml/backup_rules.xml"
resource 0x7f020001 org.example:xml/data_extraction_rules:
  (string8) "res/xml/data_extraction_rules.xml"
resource 0x7f030000 org.example:drawable/sudokura_icon_source:
  (string8) "res/drawable-nodpi-v4/sudokura_icon_source.png"
"""


def backup_tree(modern=False):
    lines = ["E: data-extraction-rules (line=1)" if modern else "E: full-backup-content (line=1)"]
    for group in (("cloud-backup", "device-transfer") if modern else (None,)):
        if group: lines.append("  E: " + group + " (line=2)")
        pad = "    " if group else "  "
        for domain in ("root", "file", "database", "sharedpref", "external"):
            lines += [pad + "E: exclude (line=3)", pad + '  A: domain="' + domain + '"', pad + '  A: path="."']
    return "\n".join(lines) + "\n"


class ResourceTests(unittest.TestCase):
    def test_renamed_resource_paths_are_resolved_by_id_and_name(self):
        renamed = RESOURCES.replace("res/xml/backup_rules.xml", "res/a.xml").replace(
            "res/xml/data_extraction_rules.xml", "res/b.xml").replace(
            "res/drawable-nodpi-v4/sudokura_icon_source.png", "res/c.png")
        self.assertEqual(audit_apk.packaged_resources(renamed, MANIFEST), {
            "fullBackupContent": "res/a.xml", "dataExtractionRules": "res/b.xml", "icon": "res/c.png"})

    def test_missing_or_ambiguous_resources_fail_closed(self):
        for resources, manifest in (("", MANIFEST), (RESOURCES, MANIFEST.replace("@0x7f020000", "@0x7f990000")),
                (RESOURCES + 'resource 0x7f020000 other:xml/other:\n  (string8) "res/other.xml"\n', MANIFEST)):
            with self.assertRaises(ValueError): audit_apk.packaged_resources(resources, manifest)

    def test_compiled_backup_policy_is_checked(self):
        for modern in (False, True):
            dump = backup_tree(modern)
            audit_apk.audit_backup_xml(dump, modern)
            for broken in (dump.replace('domain="root"', 'domain="file"'),
                           dump.replace('path="."', 'path="other"'),
                           dump.replace('E: exclude', 'E: include')):
                with self.assertRaises(ValueError): audit_apk.audit_backup_xml(broken, modern)


class IdentityTests(unittest.TestCase):
    def test_versions_derive_from_source(self):
        for minor, patch, code in ((4, 0, 1), (4, 1, 2), (5, 12, 3)):
            identity = audit_apk.expected_identity(header(1, minor, patch, code))
            self.assertEqual(identity["version_code"], code)
            self.assertEqual(identity["version_name"], f"1.{minor}.{patch}-prototype")
            audit_apk.audit_manifest(
                badging(identity["version_name"], code), MANIFEST, identity)

    def test_aapt_line_padding_is_not_a_mismatch(self):
        padded = "\n".join(line + "  " for line in badging().splitlines()) + "\n"
        audit_apk.audit_manifest(padded, MANIFEST, audit_apk.expected_identity(header()))

    def test_missing_duplicate_invalid_and_overflow_fail(self):
        for text in ("", header() + "#define SUDOKURA_VERSION_MINOR 4\n",
                     header() + "#define SUDOKURA_VERSION_ANDROID_CODE 2\n",
                     header(patch=-1), header(patch="0x1"), header(minor=100),
                     header(patch=100), header(code=0), header(code=-1),
                     header(code=2100000001), header(code='1x')):
            with self.subTest(text=text), self.assertRaises(ValueError):
                audit_apk.expected_identity(text)

    def test_unsupported_variant_fails(self):
        with self.assertRaises(ValueError):
            audit_apk.expected_identity(header(), "unknown")

    def test_production_identity_and_debuggable_refusal(self):
        identity = audit_apk.expected_identity(header(), "release")
        self.assertEqual(identity["application_id"], "org.cajapersonal.sudokura")
        self.assertEqual(identity["version_name"], "1.4.0")
        self.assertEqual(identity["version_code"], 1)
        production = badging("1.4.0").replace("org.sudokura.prototype.debug", "org.cajapersonal.sudokura")
        audit_apk.audit_manifest(production, MANIFEST, identity)
        for text, manifest in ((production + "application-debuggable\n", MANIFEST),
                (production, MANIFEST + "android:debuggable(0x0101000f)=(type 0x12)0xffffffff\n"),
                (badging(), MANIFEST)):
            with self.assertRaises(ValueError):
                audit_apk.audit_manifest(text, manifest, identity)

    def test_source_header_has_expected_candidate_version(self):
        identity = audit_apk.expected_identity(
            (audit_apk.ROOT / "src/version.h").read_text(encoding="utf-8"))
        self.assertGreaterEqual(identity["version_code"], 1)
        self.assertTrue(identity["version_name"].endswith("-prototype"))

    def test_stale_version_and_wrong_identity_fail(self):
        for text in (badging("1.3.9-prototype", 10309), badging(code=10309),
                     badging("1.4.0"), badging().replace("prototype.debug", "production")):
            with self.subTest(text=text), self.assertRaises(ValueError):
                audit_apk.audit_manifest(text, MANIFEST, audit_apk.expected_identity(header()))

    def test_package_must_be_unique_and_not_an_incidental_match(self):
        for text in (badging().replace("package:", "activity:"),
                     badging() + badging().splitlines()[0] + "\n",
                     badging().replace("versionCode='1'", "versionCode='1' versionCode='1'")):
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


class UnsignedTests(unittest.TestCase):
    def test_unsigned_and_jar_signature_refusal(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory) / "candidate.apk"
            for signer in (None, "META-INF/CERT.SF", "META-INF/CERT.RSA", "META-INF/CERT.EC"):
                with zipfile.ZipFile(apk, "w") as archive:
                    archive.writestr("AndroidManifest.xml", "fixture")
                    if signer:
                        archive.writestr(signer, "signature fixture")
                if signer:
                    with self.assertRaises(ValueError): audit_apk.audit_unsigned(apk)
                else: audit_apk.audit_unsigned(apk)

    def test_apk_signing_block_refusal(self):
        with tempfile.TemporaryDirectory() as directory:
            apk = Path(directory) / "candidate.apk"
            with zipfile.ZipFile(apk, "w") as archive:
                archive.writestr("AndroidManifest.xml", "fixture")
            data = apk.read_bytes()
            eocd = data.rfind(b"PK\x05\x06")
            offset = struct.unpack_from("<I", data, eocd + 16)[0]
            block = struct.pack("<Q", 16) + b"APK Sig Block 42"
            changed = bytearray(data[:offset] + block + data[offset:])
            struct.pack_into("<I", changed, eocd + len(block) + 16, offset + len(block))
            apk.write_bytes(changed)
            with self.assertRaises(ValueError): audit_apk.audit_unsigned(apk)


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
        flag_source = self.root / "assets/flags/LICENSE-MIT.txt"
        flag_source.parent.mkdir(parents=True)
        flag_source.write_bytes((audit_apk.ROOT / "assets/flags/LICENSE-MIT.txt").read_bytes())
        self.contents["assets/licenses/flag-icons-MIT.txt"] = flag_source.read_bytes()
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

    def test_release_renamed_xml_and_icon_archive(self):
        mapping = {"fullBackupContent": "res/a.xml", "dataExtractionRules": "res/b.xml", "icon": "res/c.png"}
        self.contents[mapping["fullBackupContent"]] = self.contents.pop("res/xml/backup_rules.xml")
        self.contents[mapping["dataExtractionRules"]] = self.contents.pop("res/xml/data_extraction_rules.xml")
        self.contents[mapping["icon"]] = self.contents.pop(self.icon)
        self.write_archive()
        self.assertEqual(len(audit_apk.audit_archive(self.apk, self.audio, mapping)), 4)
        del self.contents["res/a.xml"]
        self.write_archive()
        with self.assertRaises(ValueError): audit_apk.audit_archive(self.apk, self.audio, mapping)

    def test_missing_or_empty_notice(self):
        path = "assets/licenses/FreeType-FTL.txt"
        del self.contents[path]
        with self.assertRaises(ValueError):
            self.audit()
        self.contents[path] = b""
        with self.assertRaises(ValueError):
            self.audit()

    def test_missing_or_altered_flag_notice(self):
        notice = "assets/licenses/flag-icons-MIT.txt"
        canonical = self.contents.pop(notice)
        with self.assertRaises(ValueError):
            self.audit()
        self.contents[notice] = canonical + b"tampered"
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
                     subprocess.CompletedProcess([], 0, MANIFEST, ""),
                     subprocess.CompletedProcess([], 0, RESOURCES, ""),
                     subprocess.CompletedProcess([], 0, backup_tree(), ""),
                     subprocess.CompletedProcess([], 0, backup_tree(True), "")]
        output = io.StringIO()
        with mock.patch.object(audit_apk, "ROOT", self.root), \
             mock.patch.object(audit_apk.subprocess, "run", side_effect=responses) as run, \
             contextlib.redirect_stdout(output):
            audit_apk.main(["fake-aapt", str(self.apk), "--variant", "debug"])
        receipt = json.loads(output.getvalue())
        self.assertEqual(receipt["device_test"], "NOT_RUN")
        self.assertEqual(receipt["version_code"], 1)
        self.assertEqual(receipt["build_variant"], "debug")
        self.assertFalse(receipt["android_backup"])
        self.assertEqual(run.call_count, 5)
        self.assertEqual(run.call_args_list[0].args[0][1:3], ["dump", "badging"])

    def test_aapt_failure_is_fatal(self):
        self.write_archive()
        with mock.patch.object(audit_apk, "ROOT", self.root), \
             mock.patch.object(audit_apk.subprocess, "run", side_effect=subprocess.CalledProcessError(1, "fake-aapt")), \
             self.assertRaises(SystemExit):
            audit_apk.main(["fake-aapt", str(self.apk)])


if __name__ == "__main__":
    unittest.main()
