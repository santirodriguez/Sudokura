# Sudokura v1.1.0

This release modernizes Sudokura while keeping the lightweight C11 + SDL2 design, the original visual identity, and all three game modes.

<p align="center"><img src="https://raw.githubusercontent.com/santirodriguez/Sudokura/6f15ddc81e3600489e61fcea3b090bb00ca970de/docs/images/sudokura-v1.1.0.png" alt="Sudokura v1.1.0 game interface" width="900"></p>

Feel free to download, play, and report any issue. Feedback is especially appreciated from macOS users.

## ✅ Supported platforms

- **Linux x86_64:** ✅ Tested manually and validated as a portable AppImage.
- **Windows x86_64:** ✅ Tested manually and validated as a self-contained portable ZIP.
- **macOS Intel (x86_64):** ⚠️ Built and package-validated automatically, but not tested manually on real Mac hardware; unsigned and not notarized.
- **macOS Apple Silicon (arm64):** ⚠️ Built and package-validated automatically, but not tested manually on real Mac hardware; unsigned and not notarized.

The macOS packages contain a real `Sudokura.app` and passed automated architecture, dependency, resource, archive-integrity, and checksum validation. They are published as untested builds. Because they are unsigned, Finder may require the **Open** context action the first time they are launched.

Reports from Intel and Apple Silicon users are welcome. When reporting a macOS result, please include the Mac chip, macOS version, whether the app opened successfully, and any Gatekeeper or launch message shown.

## Downloads

- `Sudokura-v1.1.0-linux-x86_64.AppImage` — Linux x86_64
- `Sudokura-v1.1.0-windows-x86_64.zip` — Windows x86_64
- `Sudokura-v1.1.0-macos-x86_64-unsigned.zip` — macOS Intel
- `Sudokura-v1.1.0-macos-arm64-unsigned.zip` — macOS Apple Silicon
- `SHA256SUMS.txt` — SHA-256 checksums for all four packages

## What’s new in v1.1.0

- Compact responsive interface with a bounded desktop layout, native portrait layouts, a 3×3 number palette, and shared rendering/hit-test geometry.
- Complete English, natural Argentine Spanish, and Catalan localization with a visible language selector.
- Modernized cards, controls, hover/pressed states, typography, spacing, and in-app Sudokura branding while preserving the original themes and game style.
- Deterministic gameplay, generator, localization, geometry, responsive-layout, and SDL_ttf text-fit tests.
- Reproducible application icons derived from the original Sudokura artwork for Linux, Windows, macOS, and the SDL window.
- A portable Linux AppImage, dependency-complete Windows ZIP, and self-contained macOS `.app` bundles for Intel and Apple Silicon.
- Recursive runtime dependency validation, archive integrity checks, package smoke tests, inventories, architecture reports, and SHA-256 checksums.
- GitHub Actions updated for normal Linux/Windows CI, manual package previews, and tag-triggered **draft** releases.

## Notes

- The five source branding artworks remain unchanged.
- Android and iOS are not part of v1.1.0.
- The macOS builds are not signed, notarized, or manually tested on real Mac hardware.
- The release is generated as a draft and must be reviewed manually before publication.

Thank you for trying Sudokura, and happy Sudoku!
