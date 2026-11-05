# Found something odd?

Sometimes it's the puzzle. Sometimes it's a bug. If Sudokura isn't doing what you expected, a short, clear report helps us tell the difference.

You don't need to be a developer. Tell us what you were playing, what you tried, what happened and what you expected instead. Sudokura doesn't collect telemetry, so your report is how we learn about problems on your device.

## Include
- Sudokura version, OS/version, architecture, and package type.
- Game mode/difficulty and normal vs Daily when relevant.
- Seed and generator revision when they are available for the affected puzzle.
- Exact steps, expected result, actual result, and whether reopening Sudokura changes the behavior.
- A screenshot or short recording when the problem is visual and sharing it is safe.

For save/migration problems, also state the previous Sudokura version and whether recovery was offered. If reporting a touch problem, specify finger vs mouse, whether the gesture moved, and whether another finger or a window rotation interrupted it.

For Android, include the source commit/build receipt when available, device and Android version/API, portrait or landscape, navigation mode (gesture/three-button), and whether the problem occurred on background, lock, reopen, or Back. Report actual behavior rather than assuming an APK's successful build guarantees it.

## Privacy and local diagnostics
Do not post `profile.dat`, `profile.dat.bak`, legacy save files, crash dumps, home-directory paths, usernames, or other personal data unless the specific data is necessary to reproduce the problem and you have reviewed it.

For save problems, start with filenames, visible status/error text, file sizes, and checksums. Share file contents only when necessary.

## Profile location
Sudokura uses SDL's per-user preference path for organization `santirodriguez` and application `Sudokura`.

Windows normally uses `%APPDATA%\santirodriguez\Sudokura\`. Linux follows `XDG_DATA_HOME`; without a custom setting it is normally under `~/.local/share/santirodriguez/Sudokura/`.

The active profile files are normally `profile.dat`, `profile.dat.bak`, and the writer-lock file. The `games/` directory also holds individual attempts as 32-hex-character IDs with `.dat` records, recoverable `.dat.bak` copies, and `.deleted` receipts. A `games/replay.intent` marker may be present after an interrupted Replay operation. Do not delete these files independently to repair a library or profile error.

An upgrade to the v1.3.7 content format can preserve immutable `profile.dat.pre-v1.3.7.bak` copies (occasionally with a numeric suffix) and a resumable `profile.dat.v1.3.7-import.dat` intent. They are recovery evidence, not redundant temporary clutter. First migration from v1.2 also preserves `session.dat.v1.2.bak`, `preferences.dat.v1.2.bak`, and `audio-levels.dat.v1.2.bak`.

The `profile.dat` recent-result index retains up to 128 summaries, while completed exact-game records are stored separately in `games/` without that result-list cap. An imported legacy summary without a saved original board cannot be retroactively turned into a replayable exact game.

## Saved Games and Your journey

In v1.4.0, Saved Games lists explicit saves plus one latest global autosave. Achieved and Try again group finished or interrupted attempts by their verified replay ancestry and retain earlier attempt statistics. Adding notes, selecting a cell or viewing an untouched board does not start a failed attempt; submitting a digit or applying a reveal does. Backgrounding checkpoints progress without abandoning the attempt. Report missing or incorrectly grouped history separately from a missing saved board.

## Before recovery or rollback
1. Close every Sudokura instance.
2. Copy the whole profile directory somewhere safe.
3. Keep the `*.v1.2.bak` and `profile.dat.pre-v1.3.7.bak*` migration sources unchanged.
4. Back up **the whole directory**, including `games/`, `.deleted` receipts, any intent files, settings and both profile copies. Copying only `profile.dat` is insufficient.
5. Do not rename `profile.dat` to an older filename or move individual `games/` records into an older installation.
6. When reverting to v1.2 or an older v1.3.x build, use compatible data only in a separate working copy; never assume the older reader understands a newer profile or session format. Version 1.4.0 adds journey/activity metadata; backing up before upgrading is essential for a safe rollback.

A newer profile/session is not expected to become readable by an older incompatible release. Preserve the original data for a future compatible upgrade.

Android production (`org.cajapersonal.sudokura`) and the private prototype (`org.sudokura.prototype.debug`) use separate application-private data directories. Both disable Android backup/device transfer. No automatic prototype migration, desktop-save transfer or cross-store migration is promised. Production direct updates must retain the same application ID and app-signing certificate; debug builds may have incompatible ephemeral signatures. **Do not uninstall the only copy of a game to resolve an installation error**: uninstalling can delete local progress. The desktop directory-copy instructions above do not imply that Android private storage is accessible through a file manager.

## Platform details
- **Windows:** record the exact SmartScreen/reputation message if launch is warned or blocked; do not disable security protections as troubleshooting.
- **Linux:** state whether the AppImage used FUSE or `--appimage-extract-and-run`, and whether the session is X11 or Wayland when relevant.
- **macOS:** the Apple Silicon/arm64 build for macOS 15+ is experimental; record the exact macOS/Gatekeeper/quarantine message and whether the app was launched from the DMG or Applications.
- **Android:** state whether this is the signed production APK or a private debug prototype, and identify the download source. Include device/API, system-bar/cutout behavior, page size if known, and touch/audio/Save-reopen results. Full TalkBack navigation and 16 KB runtime compatibility are not verified. Store availability is separate from runtime behavior; a missing F-Droid or Google Play listing is not a game crash.
