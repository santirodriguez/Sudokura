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

    def test_targz_allows_only_in_tree_symlinks_and_hardlinks(self):
        with tempfile.TemporaryDirectory() as base:
            root = Path(base)
            archive = root / "links.tar.gz"
            folder = "SDL2-2.32.10"
            with tarfile.open(archive, "w:gz") as stream:
                directory = tarfile.TarInfo(f"{folder}/src")
                directory.type = tarfile.DIRTYPE
                stream.addfile(directory)
                file_entry = tarfile.TarInfo(f"{folder}/src/example.c")
                file_entry.size = 3
                stream.addfile(file_entry, io.BytesIO(b"yes"))
                symlink = tarfile.TarInfo(f"{folder}/src/code.c")
                symlink.type = tarfile.SYMTYPE
                symlink.linkname = "example.c"
                stream.addfile(symlink)
                hardlink = tarfile.TarInfo(f"{folder}/copy.c")
                hardlink.type = tarfile.LNKTYPE
                hardlink.linkname = f"{folder}/src/example.c"
                stream.addfile(hardlink)
            stage = root / "stage"
            fetch.extract_archive(archive, stage, {"directory": folder})
            self.assertEqual((stage / folder / "src/code.c").read_bytes(), b"yes")
            self.assertTrue((stage / folder / "src/code.c").is_symlink())
            self.assertEqual((stage / folder / "copy.c").read_bytes(), b"yes")

    def test_targz_rejects_absolute_dangling_and_cyclic_links(self):
        with tempfile.TemporaryDirectory() as base:
            root = Path(base)
            folder = "SDL2-2.32.10"
            for names in (
                [("link", "/etc/passwd")],
                [("link", "missing")],
                [("a", "b"), ("b", "a")],
            ):
                archive = root / "bad-links.tar.gz"
                with tarfile.open(archive, "w:gz") as stream:
                    directory = tarfile.TarInfo(folder)
                    directory.type = tarfile.DIRTYPE
                    stream.addfile(directory)
                    for name, linkname in names:
                        link = tarfile.TarInfo(f"{folder}/{name}")
                        link.type = tarfile.SYMTYPE
                        link.linkname = linkname
                        stream.addfile(link)
                with self.subTest(names=names):
                    with self.assertRaises(RuntimeError):
                        fetch.extract_archive(
                            archive, root / "stage", {"directory": folder})

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
