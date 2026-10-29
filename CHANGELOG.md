# Changelog

## [1.3.9]

### Direct touch and safer scrolling
- Added direct finger input using a single active touch owner and matching down/up targets. Cancelling or dragging away cannot activate a different button; real mouse, keyboard, wheel, right-click notes, audio controls, and existing desktop window layouts remain available.
- Added bounded vertical drag scrolling in Help and Saved Games, including mouse drag. Library scrolling changes only the highlighted entry; swipe gestures cannot execute Load, View, Replay, Delete, or Save.

### Early Android prototype
- Built an experimental ARM64 Android debug host from the shared C11/SDL2 engine with pinned source dependencies, bundled fonts/music/licenses, private app storage, Android lifecycle/Back handling, display insets, density-aware mobile layout, and the existing Save/recovery paths.
- Improved the phone layout with a centered gameplay wordmark, larger controls on tall screens, a localized Home action, clearer manual Save confirmation, simplified status labels, soft system-respecting touch haptics, and a bottom-positioned More panel showing only an empty board silhouette.
- Maintainer testing on a physical phone reported satisfactory general gameplay, working music, and successful manual Save/close/reopen retaining numbers, notes, and progress. Detailed device metrics and specialized edge-case tests were not individually recorded.
- The Android build remains a **debug-only prototype**, with temporary application identity, ephemeral signing, disabled backup/transfer, and no store or public APK distribution. Landscape on short surfaces, full accessibility, forced-process-death recovery, and real 16 KiB page-size execution are not claimed as accepted.

### Existing data and desktop packages
- Preserved deterministic generator revision 3, revision-2 reconstruction, normal/Daily independence, stable game/attempt identities, library records, versioned migration and deletion receipts, offline operation, languages, themes, and optional audio. No persistence-format change or forced new-game naming was introduced.
- The public desktop package contract remains Windows x64 installer and portable EXE, Linux x86_64 AppImage, and experimental macOS 15+ Apple Silicon DMG, plus checksums and Build-Info. Android is not a public release asset for v1.3.9.
- Android debug-preview and desktop CI/package checks passed at the relevant tested source commits. The optional 182-scene UI review and the repeated v1.3.8-to-v1.3.9 desktop benchmark also passed in activated push jobs; their PR-event counterparts were conditionally skipped. Documentation-only follow-up commits did not rerun executable checks.

## [1.3.8]

### Compact desktop play
- Enlarged the playable board and reorganized narrow-window controls while retaining the regular and XL desktop layouts.
- Added a keyboard- and pointer-accessible More panel for Restart, Hint, Verify, Settings, Help, and About. The panel respects independent manual, focus, and modal pause reasons and conceals the active Time Attack board.
- Kept Menu, Pause, Save, Undo, Redo, Clear, Notes, number entry, and save-error recovery directly accessible in compact gameplay.

### Pointer and Saved Games navigation
- Ordinary primary-button actions now activate on a matching release, so dragging off a button or releasing over another action cancels the press. Keyboard shortcuts, immediate board selection, right-click notes, sliders, and speaker press gestures retain their distinct behavior.
- Added visible Previous/Next page controls and a localized visible-range indicator to Saved Games, while preserving keyboard/wheel navigation, stable attempt identities, and read-only browsing of finished records.

### Compatibility and validation
- Preserved existing profile/library formats, save and migration recovery, puzzle generation revisions, Daily identity, all modes, localization, optional audio, and offline behavior.
- Corrected incremental rebuilding when the Saved Games presentation fragment changes and tightened generated-test cleanup/ignore rules.
- Extended compact-control geometry, pointer cancellation, text-fit, paging-boundary, SDL smoke, and diagnostic screenshot coverage.
- The supported desktop package formats remain the Windows installer/portable EXE, Linux AppImage, and experimental Apple Silicon macOS DMG. No Android build is included.

## [1.3.7]

### Reliable Saved Games
- Added explicit Save and a retained Saved Games browser with latest-progress Load, read-only finished-game View, exact-board practice Replay, and per-attempt Delete.
- Preserved multiple normal attempts, independent Daily originals, previous good copies, and durable deletion receipts without pruning finished games to the 128-entry recent-results limit.
- Added stable attempt IDs, bounded per-game records, checked readback, resumable pre-v1.3.7 migration, and recovery paths that distinguish unreadable, corrupt, and incompatible data.
- Expanded codec, migration cut-point, failed-write, archive, replay, Daily, and deletion tests, including larger-library scan and cancellation coverage.

### Desktop interface and accessibility
- Simplified the Home presentation and refreshed gameplay controls with clearer grouping, Save prominence, responsive spacing, and pressed/hover/focus feedback.
- Restored the complete canonical Home artwork in a dedicated gameplay sidebar header and kept compact and large layouts readable.
- Added Saved Games copy, shortcuts, and feedback across English, Español, and Català without changing deterministic puzzle revisions or optional audio behavior.

### Compatibility
- Existing compatible v1.2 and v1.3 data is retained during upgrade, with immutable pre-v1.3.7 profile sources and checked migration/recovery.
- Older summary-only results remain summaries when their exact puzzle state is unavailable; newly retained exact records can be reopened or replayed.
- The four-platform packaging contract is unchanged. macOS arm64 remains experimental; rollback to older builds requires a backup of the complete data directory.

## [1.3.6]

### Correctness and robustness
- Blocked paused or otherwise non-playable Hint/game actions from mutating board state or intercepting unrelated controls.
- Stopped failed-save Retry/Back/Discard dialogs from consuming Time Attack gameplay time while preserving nested pause ownership.
- Made gameplay shortcuts use each queued SDL key event's modifier snapshot instead of process-wide current modifier state.
- Preserved win/loss audio context through Help, About, and Settings round trips without replaying result cues.

### Accessibility and quality of life
- Raised effective minor-grid contrast against real alpha-composited board states while preserving major/minor hierarchy.
- Reworked progress text onto a stable contrasting backing instead of switching foreground near half progress.
- Exposed automatic peer-note cleanup in Settings with mouse, keyboard, and EN/ES/CA parity; the persisted default remains off.

### Validation and repository hygiene
- Expanded effective-color, text-fit, paused-interaction, clock, queued-input, result-context, Settings, and note-cleanup regressions.
- Completed exact generated-test ignores and cleanup of the crash-write test's temporary fixture.
- Kept the v1.3 persistence schema, generator revision 3, revision-2 compatibility, and supported package matrix unchanged.

## [1.3.5] - 2026-09-25

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
