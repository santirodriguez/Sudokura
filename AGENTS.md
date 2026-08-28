# Sudokura development rules

These instructions apply to the entire repository.

## Product constraints

- Keep the application in C11 using SDL2 and SDL2_ttf.
- Do not rewrite it in another language, toolkit, engine, or web framework.
- Preserve the established visual identity, game modes, keyboard/mouse interaction, and offline behavior unless an explicitly approved change requires otherwise.
- Keep runtime dependencies and packaged size low. Do not add SDL_image only to load icons or flags.
- Target Sudokura v1.2.0. The numeric version components in `version.h` are the source of truth; packaging and workflows must derive the version from them rather than maintain independent version strings.

## Engineering requirements

- Add or update automated tests alongside behavior changes.
- Keep game rules, persistence, localization, and responsive geometry testable without unnecessary architecture changes.
- Use one shared geometry source for rendering and pointer hit-testing.
- All visible controls must remain inside the usable window at supported sizes.
- Compile as C11 with `-Wall -Wextra -Wpedantic`; CI treats warnings as errors.
- Treat regressions, sanitizer findings, missing package dependencies, corrupted persistence behavior, and silent CI failures as blockers.
- Do not commit generated binaries, package archives, diagnostic UI renders, downloaded third-party binaries, or machine-specific files.

## Gameplay and persistence

- Easy, Medium, and Hard generation must remain deterministic for a seed, difficulty, and generator revision and must produce a unique solution.
- Daily Puzzle is deterministic from the local calendar date and generator revision; v1.2 defines it as Classic · Medium.
- Restart and loss Retry must preserve the exact puzzle identity.
- Autosave and Continue must fail closed on invalid or corrupt session data.
- Persistence formats are versioned and validated. Do not serialize raw C structs as an interchange format.
- Timer accounting must remain monotonic and respect independent manual, focus, modal, Home, and result pause reasons.

## Localization

- Supported languages are English, Español, and Català.
- Use general Spanish for new and maintained public strings.
- Display the language simply as `Español` without regional labels in public documentation.
- Route user-visible strings through the translation table instead of scattering literals through rendering or event-handling code.
- Preserve UTF-8 throughout.

## Branding

- Files under `assets/branding/source/` and `assets/branding/source-packed/` are the source artwork supplied for Sudokura v1.2 and must be preserved without reinterpretation.
- Use `sudokura-head.png` as the primary project and in-app identity.
- Use `sudokura-icon.png` and supplied icon-size variants as sources for platform application icons.
- Keep vendored language flags and their license/provenance under `assets/flags/`; runtime must not fetch them from the network.
- Generate derived PNG, ICO, ICNS, SDL window-icon, embedded head, and embedded flag resources reproducibly without SDL_image.
- Do not create or publish a canonical v1.2 screenshot. Only a user-supplied screenshot captured after real package testing may become the release screenshot. Automated diagnostic renders are never canonical artwork.

## Packaging

- Linux: keep an x86_64 AppImage with the real icon and bundled fallback font. Any downloaded packaging tool must be pinned to an immutable source and verified by checksum before execution.
- Windows: create a portable x86_64 ZIP with required non-system DLLs, one fallback font, embedded icon, and version metadata.
- macOS: distribute real `.app` bundles for Intel and Apple Silicon with SDL2, SDL2_ttf, resources, and bundle-relative dynamic-library paths; do not ship a bare Homebrew-linked binary.
- Do not claim signing, notarization, or manual platform testing unless it actually occurred.
- Package previews must never publish a release.
- A release tag must match the source version. Release automation may create a draft release for manual verification; publishing remains a separate explicit action.

## Completion standard

Before declaring a change complete, run the available build, test, sanitizer, UI-fit, and packaging checks that apply; summarize exact outcomes; report artifact sizes; review the net diff for accidental scope expansion; and state any validation that could not be performed.
