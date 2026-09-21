# Reporting Sudokura issues

Sudokura does not collect telemetry. A useful report is a small, reproducible description of what happened on the affected machine.

## Include
- Sudokura version, OS/version, architecture, and package type.
- Game mode/difficulty and normal vs Daily when relevant.
- Seed and generator revision when they are available for the affected puzzle.
- Exact steps, expected result, actual result, and whether reopening Sudokura changes the behavior.
- A screenshot or short recording when the problem is visual and sharing it is safe.

For save/migration problems, also state whether the installation was upgraded from v1.2 and whether recovery was offered.

## Privacy and local diagnostics
Do not post `profile.dat`, `profile.dat.bak`, legacy save files, crash dumps, home-directory paths, usernames, or other personal data unless the specific data is necessary to reproduce the problem and you have reviewed it.

For save problems, start with filenames, visible status/error text, file sizes, and checksums. Share file contents only when necessary.

## Profile location
Sudokura uses SDL's per-user preference path for organization `santirodriguez` and application `Sudokura`.

Windows normally uses `%APPDATA%\santirodriguez\Sudokura\`. Linux follows `XDG_DATA_HOME`; without a custom setting it is normally under `~/.local/share/santirodriguez/Sudokura/`.

The active v1.3 files are normally `profile.dat`, `profile.dat.bak`, and the writer-lock file. First migration from v1.2 also preserves `session.dat.v1.2.bak`, `preferences.dat.v1.2.bak`, and `audio-levels.dat.v1.2.bak`.

## Before recovery or rollback
1. Close every Sudokura instance.
2. Copy the whole profile directory somewhere safe.
3. Keep the `*.v1.2.bak` migration sources unchanged.
4. Do not rename `profile.dat` to a legacy filename.
5. When reverting to v1.2, use v1.2-compatible data only; make a working copy of a migration backup if restoration is required.

A v1.3-only session is not expected to become readable by v1.2.

## Platform details
- **Windows:** record the exact SmartScreen/reputation message if launch is warned or blocked; do not disable security protections as troubleshooting.
- **Linux:** state whether the AppImage used FUSE or `--appimage-extract-and-run`, and whether the session is X11 or Wayland when relevant.
- **macOS:** v1.3.0 arm64 is experimental; record the exact macOS/Gatekeeper/quarantine message and whether the app was launched from the DMG or Applications.
