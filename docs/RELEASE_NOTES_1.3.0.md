# Sudokura v1.3.0

Sudokura v1.3.0 is a reliability, gameplay, interface, and desktop-distribution update. It keeps the project small, native, offline, and telemetry-free while making saves safer, difficulty more explainable, player actions reversible, and package behavior more explicit.

The final v1.3.0 candidate passed automated CI/package gates and real Windows + Linux acceptance. The macOS package remains experimental because no real-device/Gatekeeper acceptance was recorded.

## Highlights
- Versioned `profile.dat` storage with independent normal/Daily slots, previous-copy recovery, writer locking, and visible save-failure handling.
- Deterministic generator revision 3 with human-technique-based difficulty labels and revision 2 compatibility for migrated v1.2 puzzle identities.
- Explainable logic Hint, explicit Verify/Reveal assistance, Undo/Redo, Clear, and reversible peer-note cleanup.
- Independent normal/Daily continuation plus bounded local result history.
- Responsive HiDPI-aware UI, keyboard focus, reduced motion, structured scrollable Help, and EN/ES/CA state terminology.
- Persistent global master/Music/FX audio controls; audio initialization/resource failure remains non-fatal.
- Windows installer + portable ZIP, Linux AppImage, and scoped macOS 15+ arm64 DMG.

## Saved data and v1.2 migration
v1.3 stores active data in a versioned `profile.dat` container with CRC and semantic validation. It keeps `profile.dat.bak` as the previous valid copy and preserves first-migration v1.2 sources as:
- `session.dat.v1.2.bak`
- `preferences.dat.v1.2.bak`
- `audio-levels.dat.v1.2.bak`

Do not rename `profile.dat` to a v1.2 filename or try to open the v1.3 container with v1.2. Back up the full profile directory before a downgrade and keep the `.v1.2.bak` files untouched.

## Gameplay and puzzle quality
Generator revision 3 evaluates deterministic puzzles using supported human-solving techniques rather than clue count alone. The current vocabulary includes singles, locked candidates, pairs/triples, and X-Wing support. Difficulty labels are engineered reasoning tiers, not a universal objective measure of human difficulty.

Every accepted generated puzzle remains independently unique. Attempt budgets are deterministic; exhaustion is reported rather than silently substituting another puzzle.

Hints are solution-blind while searching for a deduction. Previewing a Hint does not mark a game Assisted; applying a hint, Verify, or Reveal does. Undo/Redo restores visible board/note edits but not elapsed time, penalties, strikes, or Assisted state.

## Interface, languages, and audio
- English, Español, and Català remain supported.
- Help is split into Goal, Modes & rules, Board controls, Hints & verification, and Shortcuts & settings with complete compact scrolling.
- Enabled, Disabled, and Muted concepts use distinct localized vocabulary.
- `V` toggles master mute; `Shift+V` opens Music/FX controls; the speaker control provides the same short/long actions.
- Music/FX mute state and levels persist independently.

## Packages
- `Sudokura-v1.3.0-windows-x86_64-setup.exe`
- `Sudokura-v1.3.0-windows-x86_64.zip`
- `Sudokura-v1.3.0-linux-x86_64.AppImage`
- `Sudokura-v1.3.0-macos-arm64.dmg`
- `SHA256SUMS.txt` and per-platform manifests

Windows 11 x64 and Linux x86_64 final candidates were manually accepted. The Windows package is not commercially code-signed. Linux supports the AppImage `--appimage-extract-and-run` fallback when FUSE is unavailable.

The macOS 15+ arm64 DMG is ad-hoc integrity signed only, **not** Developer ID signed and **not** notarized. It remains experimental without real-Mac/Gatekeeper acceptance.

## Validation
The accepted functional candidate passed warnings-as-errors builds/tests on Linux, Windows, and macOS; Linux sanitizer coverage; generator quality/calibration; package audits; Ubuntu 24.04/Fedora 44 runtime probes; a 114-frame UI review; final desktop benchmark evidence; and real Windows/Linux manual acceptance.

Diagnostic UI renders are validation artifacts only and are not public artwork.

## Public screenshot
The canonical v1.3.0 screenshot remains pending explicit user approval. The concrete proposal is a **real packaged v1.3.0 build**, 1366×768 desktop layout, dark theme, English, Classic / Medium in progress, with a representative partially played board and the global speaker visible. It should contain only the Sudokura window and no personal information.

Do not substitute `--render-screenshots` diagnostic output for this image.

## Credits
Music: **Cozy Puzzle Jingle & Result** by **MintoDog**, from OpenGameArt under CC0. Exact mapping: [`assets/audio/README.md`](../assets/audio/README.md).

Language flags are from `lipis/flag-icons` under MIT: [`assets/flags/README.md`](../assets/flags/README.md). Sudokura is GPLv3.
