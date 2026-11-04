#!/usr/bin/env python3
"""Fetch only recorded Android build inputs; verify exact bytes before extraction."""
import hashlib
import json
import os
import posixpath
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import tarfile
import tempfile
from urllib.request import urlopen
import zipfile

ANDROID = Path(__file__).resolve().parents[1]
CACHE = ANDROID / ".cache"
LOCK = ANDROID / "dependencies.lock.json"
MAX_UNPACKED = 350 * 1024 * 1024


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def download(entry):
    downloads = CACHE / "downloads"
    downloads.mkdir(parents=True, exist_ok=True)
    output = downloads / entry["archive"]
    expected = entry["sha256"]
    if output.exists():
        if sha256(output) != expected:
            raise RuntimeError(f"Existing archive checksum mismatch: {output}")
        return output
    with tempfile.NamedTemporaryFile(dir=downloads, prefix="verified-", delete=False) as temp:
        temporary = Path(temp.name)
    try:
        with urlopen(entry["url"], timeout=120) as response, temporary.open("wb") as target:
            shutil.copyfileobj(response, target, 1024 * 1024)
        if sha256(temporary) != expected:
            raise RuntimeError(f"Downloaded archive checksum mismatch: {entry['archive']}")
        temporary.replace(output)
        return output
    finally:
        temporary.unlink(missing_ok=True)


def safe_relative(name, root):
    path = PurePosixPath(name)
    if (not name or name.startswith("/") or "\\" in name or
            path.is_absolute() or ".." in path.parts or
            path.parts[0] != root):
        raise RuntimeError(f"Unsafe archive entry: {name}")
    return Path(*path.parts)


def safe_link_target(member, root):
    """Resolve a link lexically within the verified archive's source root."""
    name = member.linkname
    if not name or name.startswith("/") or "\\\\" in name:
        raise RuntimeError(f"Unsafe archive link target: {name}")
    if member.issym():
        name = posixpath.join(posixpath.dirname(member.name), name)
    target = posixpath.normpath(name)
    return safe_relative(target, root)


def materialize_links(stage, root, links):
    """Preserve verified in-tree links; never follow links outside the source root."""
    if not links:
        return
    base = (stage / root).resolve(strict=True)
    pending = links
    for _ in range(len(links)):
        remaining = []
        progressed = False
        for member in pending:
            relative = safe_relative(member.name, root)
            target_relative = safe_link_target(member, root)
            destination = stage / relative
            source = stage / target_relative
            if not source.exists():
                remaining.append(member)
                continue
            if (destination.exists() or destination.is_symlink() or
                    not destination.parent.is_dir() or
                    not destination.parent.resolve().is_relative_to(base) or
                    not source.resolve().is_relative_to(base)):
                raise RuntimeError(f"Unsafe or duplicate archive link: {member.name}")
            if member.issym():
                destination.symlink_to(os.path.relpath(source, destination.parent))
            else:
                if not source.is_file():
                    raise RuntimeError(f"Hard link is not a file: {member.name}")
                os.link(source, destination)
            progressed = True
        if not remaining:
            return
        if not progressed:
            raise RuntimeError("Archive contains dangling or cyclic links")
        pending = remaining
    raise RuntimeError("Archive link resolution failed")


def extract_archive(archive, stage, entry):
    root = entry["directory"]
    total = 0
    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as bundle:
            for member in bundle.infolist():
                relative = safe_relative(member.filename, root)
                mode = (member.external_attr >> 16) & 0o170000
                if mode == stat.S_IFLNK:
                    raise RuntimeError("Archive symlinks are not permitted")
                total += member.file_size
                if total > MAX_UNPACKED:
                    raise RuntimeError("Archive exceeds unpacked byte budget")
                target = stage / relative
                if member.is_dir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    with bundle.open(member) as source, target.open("wb") as dest:
                        shutil.copyfileobj(source, dest)
                    if ((member.external_attr >> 16) & 0o111):
                        target.chmod(0o755)
    else:
        links = []
        with tarfile.open(archive, "r:gz") as bundle:
            for member in bundle:
                relative = safe_relative(member.name, root)
                if member.issym() or member.islnk():
                    # Defer links so an archive cannot redirect subsequent writes.
                    safe_link_target(member, root)
                    links.append(member)
                    continue
                if not (member.isfile() or member.isdir()):
                    raise RuntimeError("Archive special entries are not permitted")
                total += member.size
                if total > MAX_UNPACKED:
                    raise RuntimeError("Archive exceeds unpacked byte budget")
                target = stage / relative
                if member.isdir():
                    target.mkdir(parents=True, exist_ok=True)
                else:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    source = bundle.extractfile(member)
                    if source is None:
                        raise RuntimeError("Unreadable archive member")
                    with source, target.open("wb") as dest:
                        shutil.copyfileobj(source, dest)
                    if member.mode & 0o111:
                        target.chmod(0o755)
        materialize_links(stage, root, links)
    if not (stage / root).is_dir():
        raise RuntimeError("Archive lacks expected source root")


def prepare(name, entry):
    kind = "gradle" if name == "Gradle" else "source"
    destination = CACHE / kind / entry["directory"]
    marker = destination / ".sudokura-source-sha256"
    if destination.exists():
        if not marker.is_file() or marker.read_text().strip() != entry["sha256"]:
            raise RuntimeError(f"Unverified existing source tree: {destination}")
        print(f"{name} {entry['version']} sha256={entry['sha256']} verified-cache")
        return
    archive = download(entry)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=destination.parent, prefix="source-stage-") as temp:
        stage = Path(temp)
        extract_archive(archive, stage, entry)
        extracted = stage / entry["directory"]
        (extracted / marker.name).write_text(entry["sha256"] + "\n")
        extracted.rename(destination)
    print(f"{name} {entry['version']} sha256={entry['sha256']} extracted")


def main():
    lock = json.loads(LOCK.read_text())
    subset = ["Gradle"] if "--gradle-only" in sys.argv[1:] else list(lock)
    for name in subset:
        prepare(name, lock[name])


if __name__ == "__main__":
    main()
