# Sudokura development rules

These instructions apply to the entire repository.

## Product constraints

- Keep the application in C using SDL2 and SDL2_ttf.
- Do not rewrite it in another language, toolkit, engine, or web framework.
- Preserve the existing visual identity: colors, themes, board styling, interaction model, and game modes.
- Preserve every existing feature unless a confirmed bug requires a narrowly scoped correction.
- Keep runtime dependencies and packaged size low. Do not add SDL_image only to load icons.
- Target Sudokura v1.1.0 and centralize the version string.

## Engineering requirements

- Add automated tests before or alongside behavior changes.
- Separate testable game logic from SDL rendering only where necessary; avoid an unnecessary rewrite.
- Use one shared geometry source for rendering and hit-testing.
- All visible controls must remain inside the usable window at supported sizes.
- Compile as C11 with `-Wall -Wextra -Wpedantic` where supported.
- Treat regressions, important warnings, missing package dependencies, and silent CI failures as blockers.
- Do not commit generated binaries, downloaded third-party archives, or machine-specific files.

## Localization

- Supported languages: English, Español, and Català.
- “Español” must use natural Argentine Spanish but be displayed simply as “Español”.
- Route all user-visible strings through a translation table instead of scattering literals through rendering or event-handling code.
- Preserve UTF-8 throughout.

## Branding

- Files under `assets/branding/source/` and `assets/branding/source-packed/` are the source artwork supplied for Sudokura v1.2 and must be preserved without reinterpretation.
- Use `sudokura-head.png` as the primary project and in-app visual identity.
- Use `sudokura-icon.png` and the supplied icon-size variants as the source for platform-appropriate application icons.
- Keep the vendored language flags and their license/provenance under `assets/flags/`; runtime must not fetch them from the network.
- Generate derived PNG, ICO, ICNS, SDL window-icon, embedded head, and embedded flag resources reproducibly without SDL_image.
- Do not create or publish a canonical v1.2 screenshot until the user supplies the final screenshot after package testing.

## Packaging

- Linux: keep AppImage and use the real icon.
- Windows: create a portable ZIP with all required DLLs, one fallback font, and an embedded icon.
- macOS: distribute a real `.app` bundle with SDL2, SDL2_ttf, resources, and corrected dynamic-library paths; do not ship a bare Homebrew-linked binary.
- Do not claim signing or notarization unless it was actually performed.
- Release automation must produce checksums and a draft release for manual verification.

## Completion standard

Before declaring work complete, run the available build and test commands, summarize exact results, report artifact sizes, and list any validation that could not be performed in the current environment.
