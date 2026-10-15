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

The active profile files are normally `profile.dat`, `profile.dat.bak`, and the writer-lock file. In v1.3.7, the `games/` directory also holds individual attempts as 32-hex-character IDs with `.dat` records, recoverable `.dat.bak` copies, and `.deleted` receipts. A `games/replay.intent` marker may be present after an interrupted Replay operation. Do not delete these files independently to repair a library or profile error.

An upgrade to the v1.3.7 content format can preserve immutable `profile.dat.pre-v1.3.7.bak` copies (occasionally with a numeric suffix) and a resumable `profile.dat.v1.3.7-import.dat` intent. They are recovery evidence, not redundant temporary clutter. First migration from v1.2 also preserves `session.dat.v1.2.bak`, `preferences.dat.v1.2.bak`, and `audio-levels.dat.v1.2.bak`.

The `profile.dat` recent-result index retains up to 128 summaries, while completed exact-game records are stored separately in `games/` without that result-list cap. An imported legacy summary without a saved original board cannot be retroactively turned into a replayable exact game.

## Before recovery or rollback
1. Close every Sudokura instance.
2. Copy the whole profile directory somewhere safe.
3. Keep the `*.v1.2.bak` and `profile.dat.pre-v1.3.7.bak*` migration sources unchanged.
4. Back up **the whole directory**, including `games/`, `.deleted` receipts, any intent files, settings and both profile copies. Copying only `profile.dat` is insufficient.
5. Do not rename `profile.dat` to an older filename or move individual `games/` records into an older installation.
6. When reverting to v1.2 or an older v1.3.x build, use compatible data only in a separate working copy; never assume the older reader understands new v1.3.7 profile content.

A newer profile/session is not expected to become readable by an older incompatible release. Preserve the original data for a future compatible upgrade.

## Platform details
- **Windows:** record the exact SmartScreen/reputation message if launch is warned or blocked; do not disable security protections as troubleshooting.
- **Linux:** state whether the AppImage used FUSE or `--appimage-extract-and-run`, and whether the session is X11 or Wayland when relevant.
- **macOS:** the current v1.3.x arm64 build is experimental; record the exact macOS/Gatekeeper/quarantine message and whether the app was launched from the DMG or Applications.
