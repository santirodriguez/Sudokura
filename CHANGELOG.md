# Changelog

## [1.3.5] - Unreleased

### Fixes and polish
- Fixed the global speaker control so Hint, Pause/focus-pause, Settings, generation overlays, and native dialogs cannot leave a visible, clickable, or stale audio popup/press state behind.
- Fixed the FX unmute confirmation so the effect is played only after the FX mute state is cleared.
- Kept the established visual design while tightening audio/UI regression coverage.

### Repository and distribution
- Moved the root C sources and private headers under `src/` without changing gameplay behavior, and removed two already-retired SDL presentation fragments after fresh reference checks.
- Replaced the public Windows portable ZIP with a single-file installation-free EXE built from the same audited payload as the installer; the internal ZIP remains validation evidence only.
- Standardized the four public app filenames, added one consolidated Build-Info JSON, generated final SHA-256 checksums from the exact public artifacts, and added fail-closed release-asset validation/labels.
- Removed stale version-specific workflow-dispatch defaults; package previews/releases now require an explicit ref and resolve it to an exact commit.

## [1.3.0] - 2026-09-21

### Reliability
- Introduced versioned `profile.dat` storage with independent normal/Daily slots, previous-copy recovery, visible save-failure handling, writer exclusion, and preserved v1.2 migration backups.
- Future/incompatible data is not deleted as corrupt; Windows storage paths are UTF-8 safe.
- Fixed terminal-event ordering, repeated-input penalties, stale focus/modal pauses, external About links, and measured hot-path latency.

### Gameplay
- New puzzles use deterministic generator revision 3 with human-technique-based difficulty labels; revision 2 remains for existing v1.2 puzzle identities.
- Added explainable Hint, explicit Verify/Reveal assistance, Undo/Redo, Clear, reversible peer-note cleanup, independent normal/Daily continuation, and local result history.
- Preserved Classic/Daily hidden-solution neutrality and deterministic uniqueness; bounded generation failure is explicit instead of substituting a misleading puzzle.

### Design and accessibility
- Consolidated the presentation path and responsive geometry, added HiDPI-aware rendering, keyboard focus, reduced motion, contrast checks, and bounded text caching.
- Reworked Help into structured localized sections with complete compact scrolling.
- Audited English, Español, and Català, including distinct Enabled/Disabled/Muted semantics.
- Added persistent global master/Music/FX audio controls with mouse and keyboard parity.

### Platforms and distribution
- Windows 11 x64: portable ZIP plus per-user installer; final candidate manually accepted on real Windows.
- Linux x86_64: AppImage built on Ubuntu 22.04; Ubuntu 24.04/Fedora 44 probes and final real-Linux acceptance passed.
- macOS 15+ Apple Silicon/arm64: DMG with dependency/Mach-O audits and ad-hoc integrity signing; still experimental without real-Mac/Gatekeeper acceptance.
- v1.3 does not create new Intel Mac, older-macOS, Windows 32-bit, ARM Windows, or ARM Linux packages. Historical releases remain available.

### Compatibility and rollback
- Existing v1.2 puzzle identities remain reconstructable through generator revision 2.
- First v1.3 migration preserves immutable v1.2 source backups.
- v1.3 `profile.dat` must not be renamed or fed to the v1.2 reader. Back up the whole profile directory before downgrading.

## [1.2.0]
See [`docs/RELEASE_NOTES_1.2.0.md`](docs/RELEASE_NOTES_1.2.0.md).

## [1.1.0]
See [`docs/RELEASE_NOTES_1.1.0.md`](docs/RELEASE_NOTES_1.1.0.md).
