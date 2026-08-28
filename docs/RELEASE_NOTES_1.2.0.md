# Sudokura v1.2.0

Sudokura v1.2.0 is a substantial gameplay, persistence, branding, and interface update while keeping the project lightweight: native C11, SDL2, SDL2_ttf, and no network requirement.

The canonical v1.2 screenshot will be added after a release-candidate package has been installed and manually tested.

## What’s new

- **Easy, Medium, and Hard** deterministic puzzle generation with a unique solution.
- **Daily Puzzle**, deterministically generated from the local calendar date as Classic · Medium.
- **Autosave and Continue** for the complete active session, including board state, notes, hints, mistakes, timer, mode, difficulty, and relevant preferences.
- **Restart and Retry** preserve the exact current puzzle.
- A redesigned **Home** with mode and difficulty selectors plus New Game, Continue, and Daily Puzzle actions.
- A reorganized in-game **HUD**, graphical progress bar, completed-number feedback, and distinct hint styling.
- **Real pause** behavior with safe focus-loss handling and correct timer accounting.
- A richer result summary and a small SDL-rendered victory effect.
- Visible offline language selection for **English, Español, and Català** using bundled USA, Argentina, and Senyera flag assets.
- Updated `sudokura-head` branding and reproducibly generated application icons for Linux, Windows, macOS, and SDL.
- Versioned, CRC-protected session persistence with semantic validation and corruption quarantine.
- Expanded geometry, persistence, text-fit, sanitizer, smoke-test, and package validation.

## Downloads

The final v1.2.0 release is prepared to contain:

- `Sudokura-v1.2.0-linux-x86_64.AppImage`
- `Sudokura-v1.2.0-windows-x86_64.zip`
- `Sudokura-v1.2.0-macos-x86_64-unsigned.zip`
- `Sudokura-v1.2.0-macos-arm64-unsigned.zip`
- `SHA256SUMS.txt`

The macOS packages are unsigned. No signing or notarization is claimed.

## Validation policy

All four packages are built by the same repository automation used for release previews. Automated validation covers builds, core behavior, persistence, responsive geometry, translated text fitting, package structure, dependency closure, checksums, and smoke tests where the hosted runner supports them.

Manual release-candidate testing and the final public screenshot are separate gates before publication. Platform-specific manual-testing claims will be added only after those tests actually occur.
