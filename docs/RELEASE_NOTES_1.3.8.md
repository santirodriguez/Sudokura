Sudokura 1.3.8 makes narrow desktop windows easier to play in, adds visible Saved Games paging, and lets you cancel an ordinary button click by moving away before release.

## Compact gameplay

- In narrow windows, the Sudoku board has more usable space. The frequent controls remain visible: Menu, Pause, Save, digits, Notes, Clear, Undo, and Redo.
- **More** holds the less frequent Restart, Hint, Verify, Settings, Help, and About actions. Use Tab / Shift+Tab to change the focused option, Enter or Space to activate it, or Escape to close the panel.
- Opening More pauses gameplay without discarding an already active manual or focus pause. The active board is concealed while the panel is open, including in Time Attack.
- The existing regular and large desktop layouts remain available, with English, Español, Català, light/dark themes and reduced-motion support.

## Safer mouse interaction and Saved Games

- Ordinary buttons activate on release inside the same control, rather than immediately on mouse-down. Pressing one button and releasing elsewhere cancels the action. Board selection remains immediate; keyboard shortcuts, right-click notes, sliders, and the speaker's short/long-press gesture continue to work as before.
- **Saved Games** has visible Previous and Next page buttons plus a localized range/count label. You can browse the complete retained library without a mouse wheel or keyboard.
- Paging changes the highlighted attempt and preview, not the stored game. Load, View, Replay and Delete retain their existing behavior and confirmation/recovery protections; navigation alone does not create or remove attempts.

## Existing saves, upgrades, and rollback

- Version 1.3.8 keeps the v1.3.7 profile/library format, checked Save behavior, stable attempt IDs, versioned migration/recovery and deletion receipts. It does not change puzzle-generation identities or the deterministic Daily puzzle.
- Existing compatible v1.2 and v1.3 data retains the current checked migration and backup path. Older summary-only results still cannot reconstruct a missing original board for exact replay.
- **Before downgrading**, close Sudokura and back up the entire per-user Sudokura data directory, including `profile.dat`, all `games/` records, deletion receipts, and migration/backup files. Do not rename, delete or transplant individual records to make them readable by an older version. See [Reporting Sudokura issues](REPORTING_ISSUES.md).

## Packages and platform support

- **Windows 11 x64:** per-user installer or installation-free portable EXE; neither is commercially code-signed, so Windows SmartScreen may display a reputation warning.
- **Linux x86_64:** AppImage; when FUSE is unavailable, use `--appimage-extract-and-run`.
- **macOS 15+ Apple Silicon:** experimental DMG with ad-hoc integrity signing, not Developer ID signing or notarization. An automated package check is not a claim of real-Mac/Gatekeeper approval.
- Downloads, when published, include `SHA256SUMS.txt` and `Sudokura-1.3.8-Build-Info.json` for the matching packages. This update is desktop-only; no Android APK or store release is included.

Sudokura remains offline, without accounts, cloud synchronization or telemetry. Music and effects are optional.
