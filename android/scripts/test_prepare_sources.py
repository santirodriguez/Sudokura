#!/usr/bin/env python3
"""Offline regression tests for bounded, fail-closed Android input extraction."""
import importlib.util
import io
from pathlib import Path
import tarfile
import tempfile
import unittest
import zipfile

FILE = Path(__file__).with_name("prepare_sources.py")
spec = importlib.util.spec_from_file_location("prepare_sources", FILE)
fetch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fetch)


class SourceExtractionTests(unittest.TestCase):
    def test_relative_paths(self):
        valid = fetch.safe_relative("SDL2-2.32.10/src/example.c", "SDL2-2.32.10")
        self.assertEqual(valid.as_posix(), "SDL2-2.32.10/src/example.c")
        for bad in (
            "../escape", "/SDL2-2.32.10/file", "other/file",
            "SDL2-2.32.10/../../escape", "SDL2-2.32.10\\file",
        ):
            with self.subTest(bad=bad):
                with self.assertRaises(RuntimeError):
                    fetch.safe_relative(bad, "SDL2-2.32.10")

    def test_targz_rejects_traversal_and_symlinks(self):
        with tempfile.TemporaryDirectory() as base:
            root = Path(base)
            archive = root / "bad.tar.gz"
            with tarfile.open(archive, "w:gz") as stream:
                item = tarfile.TarInfo("SDL2-2.32.10/../../escape")
                item.size = 3
                stream.addfile(item, io.BytesIO(b"bad"))
            with self.assertRaises(RuntimeError):
                fetch.extract_archive(
                    archive, root / "stage",
                    {"directory": "SDL2-2.32.10"})
            self.assertFalse((root / "escape").exists())
            with tarfile.open(archive, "w:gz") as stream:
                item = tarfile.TarInfo("SDL2-2.32.10/link")
                item.type = tarfile.SYMTYPE
                item.linkname = "../../escape"
                stream.addfile(item)
            with self.assertRaises(RuntimeError):
                fetch.extract_archive(
                    archive, root / "stage",
                    {"directory": "SDL2-2.32.10"})

    def test_zip_rejects_traversal(self):
        with tempfile.TemporaryDirectory() as base:
            root = Path(base)
            archive = root / "bad.zip"
            with zipfile.ZipFile(archive, "w") as stream:
                stream.writestr("gradle-8.13/../../escape", b"bad")
            with self.assertRaises(RuntimeError):
                fetch.extract_archive(
                    archive, root / "stage",
                    {"directory": "gradle-8.13"})
            self.assertFalse((root / "escape").exists())

    def test_sha256(self):
        with tempfile.TemporaryDirectory() as base:
            path = Path(base) / "fixture"
            path.write_bytes(b"abc")
            self.assertEqual(
                fetch.sha256(path),
                "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            )


if __name__ == "__main__":
    unittest.main()
