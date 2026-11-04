# Sudokura 1.3.6

Sudokura 1.3.6 is a focused correctness, accessibility, and quality-of-life update on the v1.3 desktop baseline.

## What changed

- Hint and gameplay actions no longer remain interactable through pause or auxiliary-screen transitions when the board is not playable.
- Failed-save Retry/Back/Discard dialogs pause gameplay time correctly, including Time Attack near the limit, without clearing pre-existing pause reasons.
- Keyboard shortcuts now use the modifier state captured by each SDL key event, improving consistency for Shift and Ctrl/Cmd combinations.
- Win/loss audio context survives Help, About, and Settings round trips without cancelling or replaying the result cue.
- Minor-grid contrast now meets the project's functional contrast target against the real composited board states used by both themes.
- Progress text uses a stable high-contrast backing across the full progress range.
- **Automatic note cleanup** is now available directly in Settings as well as through Shift+N. The preference remains off by default and keeps its existing persisted behavior.
- English, Español, and Català help/settings copy has been updated for the exposed control and actual shortcuts.

## Saves and compatibility

v1.3.6 does not introduce a new save format or generator revision. It keeps the v1.3 profile/persistence model, deterministic generator revision 3, revision-2 compatibility for existing puzzle identities, and the existing v1.2 migration/recovery safeguards.

Automatic note cleanup remains an existing persisted preference; exposing it in Settings does not change its default or serialization.

## Platforms

The release artifact contract remains:

- Windows 11 x64 — per-user installer.
- Windows 11 x64 — single-file portable EXE with no installation.
- Linux x86_64 — AppImage.
- macOS 15+ Apple Silicon/arm64 — experimental DMG, ad-hoc integrity signed and not notarized.
- `SHA256SUMS.txt` and `Sudokura-1.3.6-Build-Info.json`.

The macOS package remains experimental until real-Mac/Gatekeeper acceptance is recorded. Windows packages are not commercially code-signed, so SmartScreen may show a reputation warning.

## Verification

The v1.3.6 candidate is required to pass the repository's core/UI suites, warnings-as-errors build, generator-quality corpus, sanitizers, effective-contrast validation, diagnostic UI renders, Linux runtime probes, Windows/Linux/macOS packaging audits, and release-bundle validation on the exact candidate head.

Automated package validation is not a substitute for any explicitly recorded real-machine acceptance. Publication remains a separate release decision.

## Credits

Music: **Cozy Puzzle Jingle & Result** by **MintoDog**, from OpenGameArt under CC0. Exact mapping: [`assets/audio/README.md`](../assets/audio/README.md).

Language flags are from `lipis/flag-icons` under MIT: [`assets/flags/README.md`](../assets/flags/README.md). Sudokura is GPLv3.
