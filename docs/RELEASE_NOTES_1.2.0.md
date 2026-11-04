# Sudokura v1.2.0

Sudokura v1.2.0 is a substantial gameplay, persistence, branding, audio, and interface update while keeping the project lightweight and offline: native C11 with SDL2, SDL2_ttf, and SDL2_mixer, with no account or network-backed puzzle service.

The final Linux release-candidate package was installed and manually tested successfully before the canonical v1.2.0 screenshot was approved and added to the repository. Windows and macOS packages were validated by the automated package pipeline; no manual launch claim is made for those platforms.

## What’s new

- **Easy, Medium, and Hard** deterministic puzzle generation with a unique solution.
- **Generator revision 2**, using deterministic randomized backtracking from an empty board before uniqueness-preserving clue removal. The generator accepts `2^64` seed values per difficulty; this describes the input space, not a guarantee that every seed produces a globally unique final puzzle.
- **Daily Puzzle**, generated deterministically from the local calendar date as Classic · Medium.
- **Classic/Daily hidden-solution neutrality:** normal entries do not expose whether they match the stored solution. Strikes and Time Attack retain the explicit correctness feedback their rules require.
- **Autosave and Continue** for active sessions, including board state, notes, hints, mistakes, timer, mode, difficulty, and relevant preferences. Completed sessions are no longer presented as resumable games.
- **Meaningful-progress confirmations:** elapsed time or moving around a fresh board alone no longer triggers a misleading “progress will be lost” warning. Replacement/Restart confirmation is reserved for actual player input, notes, hints, or mistakes/strikes.
- **Restart and Retry** preserve the exact current puzzle.
- A redesigned **Home** with mode and difficulty selectors plus New Game, Continue, and Daily Puzzle actions.
- A clearer in-game action hierarchy: **Menu** is the primary full-width action, followed by Pause/Restart, Hint/Notes/Verify, Sound/Help, the number palette and progress, with About as a footer action.
- A reorganized in-game **HUD**, graphical progress bar, completed-number feedback, larger compact branding, and a real Pause control. Classic/Daily use the neutral HUD space for the hint count instead of a meaningless error counter.
- **Physical numeric-keypad support**, including keypad scancode fallback when Num Lock changes the reported navigation keycode.
- **Responsive XL desktop layout:** maximized 1920×1080 and larger windows grow the board, sidebar, typography, and surrounding screens rather than retaining the former 720-pixel board cap. Portrait layouts remain separately tuned.
- **Real pause** behavior with safe focus-loss handling and correct timer accounting. Music also pauses while the game is manually paused or focus-paused.
- **Optional offline audio** using SDL2_mixer: background music, result jingles, and subtle programmatically generated UI/input effects. `V` remains the global mute/unmute shortcut.
- A dedicated **Audio** panel exposes independent Music and FX levels from 0–100%. The default balance is Music 20% / FX 65%, the levels persist locally, and FX adjustments provide immediate audible feedback. Result jingles follow the FX level; background loops follow the Music level.
- Returning to **Home through Menu** now starts the same Home/Clear cue used when Sudokura opens, then continues the normal Home loop. Existing victory and failure result behavior remains unchanged; Help/About keep the music context from which they were opened.
- A refreshed **About** screen with a human-readable Sudoku-scale fact, a clearly separated and carefully qualified number-puzzle/cognition note, the source study link, project/music credits, and four localized project/support links.
- **Support Sudokura** opens the language-matched support page for English, Español, or Català.
- A richer result summary and a small SDL-rendered victory effect.
- Visible offline language selection for **English, Español, and Català** using bundled USA, Argentina, and Senyera flag assets.
- Exactly two source branding masters: `sudokura-head.png` for full identity and `sudokura-512.png` for application/gameplay iconography. Linux, Windows, macOS, and SDL icon resources are derived reproducibly from the latter.
- Versioned, CRC-protected session persistence with semantic validation, explicit incompatible-generator handling, and corruption quarantine.
- Expanded geometry, meaningful-progress, persistence, seed, audio, input, text-fit, sanitizer, smoke-test, and package validation.

## Audio credit

The bundled tracks are from **Cozy Puzzle Jingle & Result** by **MintoDog**, published on OpenGameArt under **CC0**. Filename mapping is recorded in [`assets/audio/README.md`](../assets/audio/README.md).

Short button and input sounds are synthesized at runtime and do not add further media assets.

## Downloads

The final v1.2.0 release is prepared to contain:

- `Sudokura-v1.2.0-linux-x86_64.AppImage`
- `Sudokura-v1.2.0-windows-x86_64.zip`
- `Sudokura-v1.2.0-macos-x86_64-unsigned.zip`
- `Sudokura-v1.2.0-macos-arm64-unsigned.zip`
- `SHA256SUMS.txt`

The macOS packages are unsigned. No signing or notarization is claimed.

## Validation policy

All four packages are built by the same repository automation used for release previews. Automated validation covers deterministic generation, core behavior, meaningful-progress semantics, persistence, seed acquisition, numeric-keypad mapping, SDL2_mixer volume/context handling, responsive geometry including the XL tier, translated Help/About text fitting, package structure, dependency closure, checksums, and smoke tests where the hosted runner supports them.

Diagnostic UI review produces **70 non-canonical frames** across ten viewports and seven screens: Home, Play, Help, About, Result, Pause, and Audio. These frames are validation artifacts only and are never substituted for the final user-supplied release screenshot.

## Manual release-candidate validation

The final Linux RC application build (`7f0c6ec3ff7ceafb2a9b2cf1d0e834b344ede215`) was manually tested and approved. The approved canonical screenshot is stored at [`docs/images/sudokura-v1.2.0.png`](images/sudokura-v1.2.0.png). Subsequent Stage B changes only finalize documentation and screenshot placement; they do not alter the tested application binary.
