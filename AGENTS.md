# Sudokura development rules

These instructions apply to the entire repository.

## Product constraints

- Keep the application in C11 using SDL2, SDL2_ttf, and the deliberately approved SDL2_mixer audio layer.
- Do not rewrite it in another language, toolkit, engine, or web framework.
- Preserve the established visual identity, game modes, keyboard/mouse interaction, offline behavior, and optional-audio model unless an explicitly approved change requires otherwise.
- Keep runtime dependencies and packaged size low. Do not add SDL_image only to load icons or flags, and do not add media dependencies for UI effects that can be generated safely at runtime.
- Target Sudokura v1.2.0. The numeric version components in `version.h` are the source of truth; packaging and workflows must derive the version from them rather than maintain independent version strings.

## Engineering requirements

- Add or update automated tests alongside behavior changes.
- Keep game rules, persistence, localization, audio state, and responsive geometry testable without unnecessary architecture changes.
- Use one shared geometry result for rendering and pointer hit-testing, including presentation-only Home positioning.
- All visible controls must remain inside the usable window at supported sizes. Large desktop windows must use the XL geometry tier rather than merely centering the regular 720-pixel board in unused space.
- Compile as C11 with `-Wall -Wextra -Wpedantic`; CI treats warnings as errors.
- Treat regressions, sanitizer findings, missing package dependencies/assets, corrupted persistence behavior, and silent CI failures as blockers.
- Do not commit generated binaries, package archives, diagnostic UI renders, downloaded third-party binaries, or machine-specific files.

## Gameplay and persistence

- Easy, Medium, and Hard generation must remain deterministic for a seed, difficulty, and generator revision and must produce a unique solution.
- Generator revision 2 builds complete grids through deterministic randomized backtracking; do not silently reintroduce a single-pattern permutation generator or a full-grid fallback.
- Daily Puzzle is deterministic from the local calendar date and generator revision; v1.2 defines it as Classic · Medium.
- Classic and Daily must not expose hidden-solution correctness through colors, counters, palette completion, progress, or audio. Strikes and Time Attack may expose correct/incorrect feedback according to their rules.
- Restart and loss Retry must preserve the exact puzzle identity.
- Autosave and Continue must fail closed on invalid or corrupt session data. A save from an incompatible generator revision is incompatible, not corrupt.
- Persistence formats are versioned and validated. Do not serialize raw C structs as an interchange format.
- Timer accounting must remain monotonic and respect independent manual, focus, modal, Home, and result pause reasons.

## Audio

- Audio is optional. Failure to initialize SDL2_mixer, open an audio device, or load an audio asset must never prevent Sudokura from starting or preserving gameplay state.
- Bundled music provenance belongs in `assets/audio/README.md`. The v1.2 music is MintoDog's `Cozy Puzzle Jingle & Result` from OpenGameArt under CC0; preserve the original OGG payloads unless a separately approved optimization explicitly changes them.
- Keep button and input effects subtle. Runtime-generated effects are preferred over adding more media files for simple clicks/cues.
- Correct/incorrect audio cues must obey `game_mode_reveals_correctness()` so Classic and Daily remain neutral.
- Audio enable/disable is a persisted preference and must remain backward compatible with existing v1.2 release-candidate preference files.

## Localization

- Supported languages are English, Español, and Català.
- Use general Spanish for new and maintained public strings.
- Display the language simply as `Español` without regional labels in public documentation.
- Route user-visible strings through the translation table instead of scattering literals through rendering or event-handling code.
- Preserve UTF-8 throughout.

## Branding

- `sudokura-head.png` is the full project identity for Home, About, Pause, result, and public documentation. Use the compact application icon in gameplay chrome.
- `assets/branding/source/android-chrome-512x512.png` is the canonical application-icon master for generated PNG sizes, Windows ICO, macOS ICNS, and the SDL window icon.
- The retired white-background `favicon-16x16.png`, `favicon-32x32.png`, and `favicon.ico` variants must not be reintroduced.
- Other retained files under `assets/branding/source/` and `assets/branding/source-packed/` are source artwork supplied for Sudokura v1.2 and must not be reinterpreted.
- Keep vendored language flags and their license/provenance under `assets/flags/`; runtime must not fetch them from the network.
- Generate derived PNG, ICO, ICNS, SDL window-icon, embedded head, and embedded flag resources reproducibly without SDL_image.
- Do not create or publish a canonical v1.2 screenshot. Only a user-supplied screenshot captured after real package testing may become the release screenshot. Automated diagnostic renders are never canonical artwork.

## Packaging

- Linux: keep an x86_64 AppImage with the real icon, fallback font, SDL2_mixer dependency closure, and the four OGG assets. Any downloaded packaging tool must be pinned to an immutable source and verified by checksum before execution.
- Windows: create a portable x86_64 ZIP with required non-system DLLs, SDL2_mixer, the audio assets, one fallback font, embedded icon, and version metadata.
- macOS: distribute real `.app` bundles for Intel and Apple Silicon with SDL2, SDL2_ttf, SDL2_mixer, transitive non-system dylibs, audio resources, and bundle-relative dynamic-library paths; do not ship a bare Homebrew-linked binary.
- Do not claim signing, notarization, or manual platform testing unless it actually occurred.
- Package previews must never publish a release.
- A release tag must match the source version. Release automation may create a draft release for manual verification; publishing remains a separate explicit action.

## Completion standard

Before declaring a change complete, run the available build, test, sanitizer, UI-fit, audio, and packaging checks that apply; summarize exact outcomes; report artifact sizes when packages are part of the gate; review the net diff for accidental scope expansion; and state any validation that could not be performed.
