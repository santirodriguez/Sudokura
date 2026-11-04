Sudokura 1.3.9 adds direct touch support and safer drag scrolling on desktop, while introducing an early **Android debug prototype for testing**. Desktop distribution remains available in the existing package formats; Android is not yet a public download or store release.

## Touch and navigation

- Tap with a finger on supported desktop touchscreens using the same gameplay controls as mouse/keyboard. A gesture activates only its original target; moving away or dragging does not trigger another action.
- Drag to scroll Help or browse Saved Games, with a mouse or finger. Browsing changes the selected entry and preview but **does not** load, replay, delete, or overwrite a saved attempt.
- Existing keyboard shortcuts, right-click notes, mouse wheel and visible Saved Games paging continue to work.

## Android prototype — testing only

- A native ARM64 debug build uses the shared Sudoku engine, a portrait-oriented layout, larger touch controls, localized navigation and Save feedback, optional music/effects, and app-private progress storage.
- On a physical phone, the maintainer reported working music and successful manual Save followed by app close/reopen with numbers, notes and progress retained; the refreshed layout received positive feedback. This is functional feedback, not certification of every device or lifecycle edge case.
- The prototype is **not included among this release's public download assets** and has no stable production signing, public package identity, supported upgrade path, Play Store listing or guaranteed backup/transfer. Do not uninstall a preview containing progress to bypass a mismatched debug signature.
- Very short landscape surfaces require returning to portrait. Full accessibility, OS-process-kill recovery, device performance measurements and real 16 KiB-page execution are not claimed verified. A public Android release remains a later, separately validated milestone.

## Saves and compatibility

- The existing on-disk profile/library formats, generator revisions, Daily identities, attempt IDs, manual Save, automatic checkpoints, retained records, and recovery/deletion protections are unchanged. Repeated Save continues to update the same attempt.
- Compatible v1.2/v1.3 desktop data retains its existing checked migration/recovery path. Older summary-only results cannot acquire a replayable original board retrospectively.
- **Before downgrading**, close Sudokura and back up the complete desktop per-user data directory, including `profile.dat`, all `games/` files, deletion receipts, and migration/backup sources. Do not rename or individually move records to force an older build to open them. See [Reporting Sudokura issues](https://github.com/santirodriguez/Sudokura/blob/main/docs/REPORTING_ISSUES.md) for details.

## Desktop packages

- **Windows 11 x64:** per-user installer and single-file portable EXE. Commercial code signing is not included, so SmartScreen may display a reputation warning.
- **Linux x86_64:** AppImage; use `--appimage-extract-and-run` when FUSE is unavailable.
- **macOS 15+ Apple Silicon:** experimental DMG with ad-hoc integrity signing, without Developer ID signing or notarization. Automated package validation is not a claim of real-device Gatekeeper approval.
- When published, the four desktop applications are accompanied by `SHA256SUMS.txt` and the matching `Sudokura-1.3.9-Build-Info.json` for download verification.

Sudokura remains offline, without accounts, cloud synchronization or telemetry. Music and effects remain optional.
