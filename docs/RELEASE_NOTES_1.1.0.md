# Sudokura v1.1.0 release notes

## Highlights

- Compact, responsive play layout with a 3×3 palette and shared render/hit-test geometry, validated from 640×480 through 1920×1080.
- Complete internal English, Argentine Spanish, and Catalan translation tables with a visible keyboard-accessible language selector.
- Deterministic tests for board generation and uniqueness, clues/fixed cells, moves, clearing, notes, hints, conflicts, solved state, all mode end conditions, localization completeness, and geometry.
- Reproducible real icons derived from `Sudokura05.png`, including PNG, ICO, ICNS, and an SDL RGBA resource without SDL_image.
- Reworked CI and packaging: AppImage, dependency-validated Windows portable ZIP, and self-contained unsigned macOS arm64/x86_64 `.app` bundles.
- Centralized version reporting and C11 `-Wall -Wextra -Wpedantic` builds.

## Distribution note

macOS packages are unsigned and not notarized. All release artifacts require manual smoke testing before the draft release is published. SHA-256 checksums accompany packages. Android and iOS are not part of this release.
