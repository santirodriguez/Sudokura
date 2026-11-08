#!/usr/bin/env python3
"""Instrument a pinned, disposable Android source checkout; never change published sources."""
import argparse
import hashlib
import json
from pathlib import Path

BASE_COMMIT = "c1b3eb31e82bcc06a078bc10e10f342aa8329e25"
TAG = "SUDOKURA_LAB"


def log_line(message):
    return ("#if defined(__ANDROID__)\n"
            "  __android_log_print(ANDROID_LOG_INFO, \"SUDOKURA_LAB\",\n"
            f"      {message});\n"
            "#endif\n")


PATCHES = {
    "src/sudokura_sdl/01_runtime.inc": {
        "blob": "1924610975bf8c8b5faca2a281417230da4bb482",
        "changes": [
            ("#include <SDL2/SDL.h>\n", "#include <SDL2/SDL.h>\n#if defined(__ANDROID__)\n#include <android/log.h>\n#endif\n"),
            ('  char*base=SDL_GetPrefPath("santirodriguez","Sudokura");\n  if(!base)return;\n',
             '  char*base=SDL_GetPrefPath("santirodriguez","Sudokura");\n'
             '  if(!base){\n' + log_line('"init prefpath=missing"') + '    return;\n  }\n'),
            ('  if(!paths_ok){SDL_free(base);return;}\n',
             '  if(!paths_ok){\n' + log_line('"init paths=invalid"') +
             '    SDL_free(base);return;\n  }\n'),
            ('  p->lock_status=store_writer_lock_acquire(base,&p->writer_lock);\n',
             '  p->lock_status=store_writer_lock_acquire(base,&p->writer_lock);\n' +
             log_line('"init lock_status=%d",(int)p->lock_status')),
            ('      p->profile_status=STORE_IO_ERROR;\n      p->startup_stage=PERSISTENCE_STARTUP_DONE;\n      return true;\n',
             '      p->profile_status=STORE_IO_ERROR;\n' + log_line('"startup stage=LOAD reason=missing_hot status=%d",(int)STORE_IO_ERROR') +
             '      p->startup_stage=PERSISTENCE_STARTUP_DONE;\n      return true;\n'),
            ('  if (status==STORE_CANCELLED) {\n    p->startup_stage=PERSISTENCE_STARTUP_LOAD;\n',
             log_line('"startup stage=%s store_status=%d",'
                      'p->startup_stage==PERSISTENCE_STARTUP_LOAD?"LOAD":'
                      'p->startup_stage==PERSISTENCE_STARTUP_UPGRADE?"UPGRADE":'
                      'p->startup_stage==PERSISTENCE_STARTUP_RECONCILE?"RECONCILE":"UNKNOWN",(int)status') +
             '  if (status==STORE_CANCELLED) {\n    p->startup_stage=PERSISTENCE_STARTUP_LOAD;\n'),
        ]
    },
    "src/game_migration.c": {
        "blob": "34c6660cff6c8e5edb57c4d7d336db8bd7a2bc68",
        "changes": [
            ('#include <string.h>\n',
             '#include <string.h>\n#if defined(__ANDROID__)\n#include <android/log.h>\n#endif\n'),
            ('    if (receipt != STORE_NOT_FOUND) return receipt;\n    GameLibraryRecord disk;\n',
             '    if (receipt != STORE_NOT_FOUND) {\n' +
             log_line('"reconcile receipt slot=%u status=%d",index,(int)receipt') +
             '      return receipt;\n    }\n    GameLibraryRecord disk;\n'),
            ('    if (status != STORE_OK) return status;\n    if (disk.kind != GAME_RECORD_EXACT ||\n',
             '    if (status != STORE_OK) {\n' +
             log_line('"reconcile record_load slot=%u status=%d",index,(int)status') +
             '      return status;\n    }\n    if (disk.kind != GAME_RECORD_EXACT ||\n'),
            ('    if (disk.revision < slot->revision) return STORE_IO_ERROR;\n',
             '    if (disk.revision < slot->revision) {\n' +
             log_line('"reconcile stale slot=%u disk_revision=%llu slot_revision=%llu",'
                      'index,(unsigned long long)disk.revision,(unsigned long long)slot->revision') +
             '      return STORE_IO_ERROR;\n    }\n'),
        ]
    },
    "src/store_io.c": {
        "blob": "7455ab0a215ed47b56ba9d9ad0398a95280f8501",
        "changes": [
            ('#include <errno.h>\n',
             '#include <errno.h>\n#if defined(__ANDROID__)\n#include <android/log.h>\n#endif\n'),
            ('  if (fd < 0) return STORE_LOCK_ERROR;\n',
             '  if (fd < 0) {\n' + log_line('"lock open errno=%d",errno') +
             '    return STORE_LOCK_ERROR;\n  }\n'),
            ('  if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {\n    close(fd);\n',
             '  if (fcntl(fd, F_SETFD, FD_CLOEXEC) != 0) {\n'
             '#if defined(__ANDROID__)\n    int fd_error=errno;\n#endif\n'
             '    close(fd);\n' + log_line('"lock fcntl errno=%d",fd_error')),
            ('    int error = errno;\n    close(fd);\n',
             '    int error = errno;\n    close(fd);\n' +
             log_line('"lock flock errno=%d",error')),
        ]
    },
}


def blob_sha(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()


def prepare(source):
    """Validate every unchanged pinned source before writing any scratch file."""
    planned = {}
    for rel, spec in PATCHES.items():
        path = source / rel
        raw = path.read_bytes()
        if blob_sha(raw) != spec["blob"]:
            raise RuntimeError("Unexpected source blob: " + rel)
        text = raw.decode("utf-8")
        for before, after in spec["changes"]:
            if text.count(before) != 1:
                raise RuntimeError("Ambiguous or missing patch anchor: " + rel)
            text = text.replace(before, after)
        # All trace statements contain only literal stage, slot, status, or errno.
        new = text.encode("utf-8")
        if TAG.encode("ascii") not in new:
            raise RuntimeError("Diagnostic tag missing: " + rel)
        planned[rel] = (path, new, spec["blob"])
    return planned


def apply(source, manifest):
    planned = prepare(source)
    for _, (path, new, _) in planned.items():
        path.write_bytes(new)
    output = {
        "kind": "LAB_INSTRUMENTED_NOT_PUBLISHED_APK",
        "base_commit": BASE_COMMIT,
        "changes_are_scratch_only": True,
        "production_signer": "NOT_USED",
        "version2_install": "NOT_RUN",
        "traced_paths": {rel: {"original_blob": old, "instrumented_sha256": hashlib.sha256(new).hexdigest()}
                         for rel, (_, new, old) in sorted(planned.items())}
    }
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text(json.dumps(output, sort_keys=True, indent=2) + "\n")


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--source", type=Path, required=True)
    p.add_argument("--manifest", type=Path, required=True)
    args = p.parse_args()
    apply(args.source, args.manifest)
