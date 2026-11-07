#!/usr/bin/env python3
"""Offline negative tests for desktop license payload verification."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import zipfile

SCRIPT = Path(__file__).resolve().parents[1] / "packaging/ci/check_flag_notice.py"
spec = importlib.util.spec_from_file_location("check_flag_notice", SCRIPT)
check = importlib.util.module_from_spec(spec)
spec.loader.exec_module(check)

NOTICE = (b"The MIT License (MIT)\nCopyright (c) 2013 Panayiotis Lipiridis\n"
          b"Permission is hereby granted, free of charge\n")


class FlagNoticeTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.source = self.root / "LICENSE-MIT.txt"
        self.source.write_bytes(NOTICE)
        self.payload = self.root / "FLAG-ICONS-LICENSE-MIT.txt"
        self.payload.write_bytes(NOTICE)

    def test_file_exact_copy(self):
        check.verify(self.payload, source=self.source)

    def test_file_missing_and_modified(self):
        self.payload.write_bytes(NOTICE + b"altered\n")
        with self.assertRaises(ValueError):
            check.verify(self.payload, source=self.source)
        self.payload.unlink()
        with self.assertRaises(FileNotFoundError):
            check.verify(self.payload, source=self.source)

    def test_archive_exact_copy_missing_and_modified(self):
        archive = self.root / "package.zip"
        with zipfile.ZipFile(archive, "w") as output:
            output.writestr(self.payload.name, NOTICE)
        check.verify(archive, self.payload.name, self.source)
        with zipfile.ZipFile(archive, "w") as output:
            output.writestr(self.payload.name, b"tampered")
        with self.assertRaises(ValueError):
            check.verify(archive, self.payload.name, self.source)
        with zipfile.ZipFile(archive, "w") as output:
            output.writestr("other.txt", b"not the notice")
        with self.assertRaises(KeyError):
            check.verify(archive, self.payload.name, self.source)

    def test_canonical_notice_must_retain_attribution(self):
        self.source.write_bytes(b"Permission is hereby granted")
        with self.assertRaises(ValueError):
            check.verify(self.payload, source=self.source)


if __name__ == "__main__":
    unittest.main()
