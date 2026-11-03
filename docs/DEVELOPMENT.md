# Sudokura development guidelines

This document describes the project's maintained technical, gameplay, and release conventions. Release-specific scopes and decisions belong to their respective plans and blueprints.

## Product constraints

- Keep the application in C11 using SDL2, SDL2_ttf, and the deliberately approved SDL2_mixer audio layer.
- Do not rewrite it in another language, toolkit, engine, or web framework.
- Preserve the established visual identity, game modes, keyboard/mouse interaction, offline behavior, and optional-audio model unless an explicitly approved change requires otherwise.
- Keep runtime dependencies and packaged size low. Do not add SDL_image only to load icons or flags, and do not add media dependencies for UI effects that can be generated safely at runtime.
- The numeric version components in `src/version.h` remain the source of truth; packaging and workflows must derive the version from them rather than maintain independent version strings. Refer to the current release blueprint for the intended next version.

## Engineering requirements

- Add or update automated tests alongside behavior changes.
- Keep game rules, persistence, localization, audio state, input mapping, and responsive geometry testable without unnecessary architecture changes.
- Use one shared geometry result for rendering and pointer hit-testing, including presentation-only Home positioning.
- All visible controls must remain inside the usable window at supported sizes. Large desktop windows must use the XL geometry tier rather than merely centering the regular 720-pixel board in unused space.
- Compile as C11 with `-Wall -Wextra -Wpedantic`; CI treats warnings as errors.
- Treat regressions, sanitizer findings, missing package dependencies/assets, corrupted persistence behavior, and silent CI failures as blockers.
- Do not commit generated binaries, package archives, diagnostic UI renders, downloaded third-party binaries, or machine-specific files.

## Gameplay and persistence

- Easy, Medium, and Hard generation must remain deterministic for a seed, difficulty, and generator revision and must produce a unique solution.
- Generator revision 3 is the current deterministic generator. Revision 2 remains supported only to reconstruct compatible existing puzzle identities; do not silently reintroduce a single-pattern permutation generator or a full-grid fallback.
- Daily Puzzle is deterministic from the local calendar date and generator revision; v1.2 defines it as Classic · Medium.
- Classic and Daily must not expose hidden-solution correctness through colors, counters, palette completion, progress, or audio. Strikes and Time Attack may expose correct/incorrect feedback according to their rules.
- Restart and loss Retry must preserve the exact puzzle identity.
- Autosave and Continue must fail closed on invalid or corrupt session data. A save from an incompatible generator revision is incompatible, not corrupt.
- Persistence formats are versioned and validated. Do not serialize raw C structs as an interchange format.
- Current journey storage writes profile content v3/session blob v4 and game record v2, with readers for prior versions. Activity and explicit-Save flags are distinct from automatic replay retention; Interrupted is distinct from a rule-based loss. Journey families follow parent attempt IDs and retain their individual outcomes.
- Saved Games uses a bounded v1.3.7 profile (16 KiB maximum), separate exact or legacy-summary game records (8 KiB maximum each), stable attempt IDs, and durable latest-progress Save. A repeat Save updates the same attempt; New and Replay create distinct identities. The 128-entry limit applies to recent-result summaries, not the permanent finished-game library.
- Migration to v1.3.7 preserves the immutable pre-upgrade profile and resumable intent; do not overwrite recoverable copies, silently downgrade an unknown format to corrupt, or import the same history twice. A deleted game's durable receipt must prevent reappearance from backups, old profiles, or migration sources.
- Startup loads, upgrades and reconciles on separate main-loop turns. Controlled migration callbacks only inspect pending context between checked operations; they never dispatch input or enter another writer. On cancellation, reload the primary before resuming the immutable intent. Keep Home and unrelated saves gated until the complete profile and pending lifecycle events are reconciled. Individual file operations remain synchronous; this does not guarantee a latency bound for stalled storage.
- Explicit startup recovery publishes only the verified backup, frozen import intent or reconstructed collection checkpoint before entering that same startup pipeline. Collection routing/rebuilding observes cancellation between scan batches/records and before publication. Cancellation before publication keeps consent gated; successful restoration never enables a partially migrated profile.
- `geometry_mobile_landscape_play` composes gameplay fields only, including 48dp actionable targets and separate progress/status rows at 360dp or taller. Its secondary-screen fields are zero; `geometry_compute_mobile` composes the complete shell over it and accepts usable 600x320dp landscape as well as 360x600dp portrait. At 320–359dp usable height it removes the decorative gameplay title and lets a transient toast replace progress, retaining all HUD fields and 48dp actions. Smaller surfaces retain the paused minimum-size fallback. Do not treat host geometry/render tests as physical-device or TalkBack acceptance.
- Modal body layout is shared by rendering and hit testing. Overflow has explicit 48dp up/down controls plus keyboard/wheel scrolling; scrolling never chooses a decision, changes its focus, releases modal pause or passes synthetic touch-mouse events through. Keep response buttons fixed and content clipped to its bounded viewport.
- Mobile Settings uses one shared geometry for touch, focus and drawing: 48dp controls, a vertical portrait composition and two columns on short wide surfaces, with wrapped button labels at a 13px minimum. Mobile Home composes landscape fields over the shared gameplay geometry. The runtime selects mobile layouts once through Gfx; host SDL smoke selects that same route for capture/render checks without claiming Android density/device emulation.
- Mobile Help/About use shared body, link and footer geometry with explicit 48dp scroll controls; About keeps 18px wrapped copy in a bounded viewport. Mobile Library shares its 48dp action/pager and whole 60dp row bounds with input, keeping short-wide previews inside the detail pane. Results/Pause and Hint actions also retain 48dp controls. Horizontal gameplay uses readable HUD rows and larger Save/Strict text targets; Undo/Redo use direction symbols in the narrow toolbar. Host geometry/text/render checks do not replace device or TalkBack acceptance.
- Mobile Help selects touch-first EN/ES/CA content and fixed 18px body text, with a bounded scroll viewport. Desktop keyboard documentation remains separate. Home landscape centers its setup/resume groups; paused actions use a width-capped group.
- Android font scale follows display density, independently of rounded usable-dp bounds. A rotation at unchanged density must not reopen all fonts because of sub-dp rounding.
- The official project site is https://sudokura.cajapersonal.org/ . Donation actions use https://cajapersonal.org/donate/ (EN), /donaciones/ (ES), and /donacions/ (CA), through the exact URL allowlist.
- Clear synthetic `SDL_Event` unions with `SDL_zero` before setting their event type and fields. C11 `{0}` initializes the first union member, not every event variant; unused keyboard repeat/mouse metadata must not depend on compiler-specific union padding initialization. Keep real SDL dispatch and focus assertions enabled across compiler/platform builds.
- Android semantics use bounded immutable snapshots and one pending semantic command. Only SDL's owner rebuilds nodes and revalidates screen generation, attempt, current enabled state, layout and record identities before calling the existing actions. Java owns traversal/hover focus only. Paused, suspended, unavailable-renderer and covered boards publish no cells; cell labels use visible entries, givens, notes and visible conflicts, never hidden solutions. Material changes publish once; a generation-only rebind does not announce timer updates. Host semantic smoke is not TalkBack/device acceptance.
- Timer accounting must remain monotonic and respect independent manual, focus, modal, Home, and result pause reasons.

## Audio

- Audio is optional. Failure to initialize SDL2_mixer, open an audio device, or load an audio asset must never prevent Sudokura from starting or preserving gameplay state.
- Bundled music provenance belongs in `assets/audio/README.md`. The v1.2 music is MintoDog's `Cozy Puzzle Jingle & Result` from OpenGameArt under CC0; preserve the original OGG payloads unless a separately approved optimization explicitly changes them.
- Keep button and input effects subtle. Runtime-generated effects are preferred over adding more media files for simple clicks/cues.
- Correct/incorrect audio cues must obey `game_mode_reveals_correctness()` so Classic and Daily remain neutral.
- Audio enable/disable is a persisted preference and must remain backward compatible with existing v1.2 release-candidate preference files.
- Preserve the established startup, win, and loss jingles. Returning to Home through Menu from unfinished gameplay intentionally plays the same Home/Clear cue used at startup and then continues the normal Clear loop; Help, About, and Audio retain their prior context, and enabled/available audio must not remain accidentally silent after screen transitions.

## Localization

- Supported languages are English, Español, and Català.
- Keep new and maintained Spanish public strings consistent with the product's established localization conventions.
- Display the language simply as `Español` without regional labels in public documentation.
- Route user-visible strings through the translation table instead of scattering literals through rendering or event-handling code.
- Preserve UTF-8 throughout.

## Branding

- Exactly two source branding masters are canonical for v1.2: `assets/branding/source/sudokura-head.png` and `assets/branding/source/sudokura-512.png`.
- `sudokura-head.png` is the full project identity for Home, the gameplay sidebar header, About, Pause, result, and public documentation.
- `sudokura-512.png` is the compact/application icon master for generated PNG sizes, Windows ICO, macOS ICNS, Linux packaging, and the SDL window icon. Gameplay's sidebar header uses the proportional full head artwork, not an extra typeset name or a squeezed square icon.
- Do not reintroduce retired favicon, Android, packed, historical, or alternate logo masters into the active branding pipeline.
- Keep vendored language flags and their license/provenance under `assets/flags/`; runtime must not fetch them from the network.
- Generate derived PNG, ICO, ICNS, SDL window-icon, embedded head, and embedded flag resources reproducibly without SDL_image.
- The canonical and historical screenshots are described in [Documentation images](images/README.md). Preserve approved screenshot versions and historical provenance; diagnostic UI renders are never canonical artwork, and the accepted image must not be replaced without explicit approval.

## Packaging

- Linux: keep an x86_64 AppImage with the real icon, fallback font, SDL2_mixer dependency closure, and the four OGG assets. Any downloaded packaging tool must be pinned to an immutable source and verified by checksum before execution.
- Windows: keep the per-user x86_64 installer and produce a single-file installation-free portable EXE around the same audited payload. An internal ZIP may remain as validation/staging evidence, but it is not the public portable artifact. Preserve required non-system DLLs, SDL2_mixer, audio assets, one fallback font, embedded icon, version metadata, and the existing AppData profile location.
- macOS v1.3.x candidate: target macOS 15+ on Apple Silicon/arm64 only, ship a real `.app` inside a simple DMG with SDL2, SDL2_ttf, SDL2_mixer, transitive non-system dylibs, audio resources, bundle-relative dynamic-library paths, complete Mach-O audit, and explicit ad-hoc integrity signing. Do not generate a new Intel candidate by inertia; historical Intel/older-macOS releases remain available but are not newly maintained.
- Do not claim signing, notarization, or manual platform testing unless it actually occurred.
- Package previews must never publish a release.
- Public release filenames are derived from the source version and use the stable contract: `Sudokura-<version>-Windows-x64-Setup.exe`, `Sudokura-<version>-Windows-x64-Portable.exe`, `Sudokura-<version>-Linux-x64.AppImage`, `Sudokura-<version>-macOS-arm64.dmg`, `SHA256SUMS.txt`, and `Sudokura-<version>-Build-Info.json`. The public Windows portable ZIP is retired; an internal ZIP may remain only as validation evidence.
- A release tag must match the source version. Release automation may create a draft release for manual verification; publishing remains a separate explicit action.

## Completion standard

Before declaring a change complete, run the available build, test, sanitizer, UI-fit, audio, input, and packaging checks that apply; summarize exact outcomes; report artifact sizes when packages are part of the gate; review the net diff for accidental scope expansion; and state any validation that could not be performed.
