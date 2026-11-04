Sudokura 1.3.7 adds a durable Saved Games library and a clearer, more comfortable desktop interface while preserving offline play and existing puzzle identities.

## Saved games and progress

- **Save** stores the latest numbers, notes, time, and state of the current attempt. Repeated saves update that attempt instead of creating duplicates.
- **Saved Games** lets you return to retained in-progress attempts, review finished results without modifying them, start a new practice Replay from an exact finished game, and explicitly Delete an individual attempt after confirmation.
- Completed game records are retained independently of the shorter recent-results list. Starting a new game no longer has to discard previously retained progress.
- Normal games and Daily puzzles remain independent. Today's original Daily is not recreated merely because you open it again, and a Replay does not replace its original result.
- Saved progress is verified before the application reports a successful Save. Recoverable copies and interrupted operations are handled conservatively instead of silently replacing readable user data.

## Interface and controls

- A simpler Home screen and a refreshed gameplay sidebar improve control spacing, visual hierarchy, and Save visibility.
- The gameplay header uses the complete original Sudokura logo, proportionally centered above the status and controls.
- Buttons have consistent hover, pressed, and keyboard-focus feedback, with layouts adapted to compact and large windows.
- Use **Ctrl+S** to Save and **Ctrl+O** to open Saved Games on Windows/Linux (Cmd equivalents on macOS). Keyboard and mouse remain supported throughout the game library.
- English, Español, and Català include the Saved Games interface and related guidance.

## Existing data, migration, and rollback

Compatible v1.2 and v1.3 data is upgraded with preserved source/backup copies. The v1.3.7 library stores individual game attempts in the user's existing Sudokura application-data directory; no account or network connection is required.

Earlier history entries that only contain a result summary cannot gain an original board retrospectively. They may remain visible as summaries but cannot offer an exact-board Replay. This is different from newly retained exact games.

**Before reverting to an older version**, close Sudokura and back up the **entire** application-data directory, including `profile.dat`, all `games/` records, and migration/backup files. Older builds are not guaranteed to understand the updated v1.3.7 profile content. Do not rename or delete the newer files to force a downgrade. See [reporting issues and rollback](https://github.com/santirodriguez/Sudokura/blob/main/docs/REPORTING_ISSUES.md) for storage details.

## Packages and support

- Windows 11 x64: per-user installer or installation-free portable EXE; both use the same user profile. The builds are not commercially code-signed, so SmartScreen may show a reputation warning.
- Linux x86_64: AppImage; if FUSE is unavailable, use `--appimage-extract-and-run`.
- macOS 15+ Apple Silicon/arm64: **experimental** DMG, ad-hoc signed but not Developer ID signed or notarized; real-Mac/Gatekeeper acceptance is not claimed.

Downloads are accompanied by `SHA256SUMS.txt` and a matching `Sudokura-1.3.7-Build-Info.json`.

Sudokura remains offline, without telemetry; audio and all application data stay under the user's control.
