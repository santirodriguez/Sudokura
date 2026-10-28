# Sudokura: desktop evolution and Android release plan

Status: Active roadmap; v1.3.8 published; v1.3.9 Phase 0 complete. Implementation and publication remain separate decisions.
Reviewed: 2026-10-26.
Current published baseline: `4d2c8dbddbe906e5dd714a2f6691dd2760e98072` (`main` and `v1.3.8` at review).
Next-release specification: [v1.3.9 blueprint](V1.3.9_BLUEPRINT.md).
Historical v1.3.8 specification: [v1.3.8 blueprint](V1.3.8_BLUEPRINT.md), originally based on v1.3.7 `483edc8ea3f93698f7781e7fe933a9d996f93fdf`.

## Outcome and constraints

Deliver a publishable Android version no later than v1.5.0, preferably v1.4.0 if device evidence supports it. This is a release horizon, not a date or a promise of store approval. A debug APK alone does not meet it. Keep improving the desktop game throughout the work.

Keep the same repository and C11 engine with SDL2, SDL2_ttf and SDL2_mixer. Add `android/` when the first real Android build is made. Do not fork gameplay, rewrite in another framework, migrate to SDL3 by anticipation, reorganize the source tree wholesale, or build a generic platform framework. A narrow Android host and the platform seams actually exercised by it are sufficient.

Preserve all modes, deterministic generation revision 3, revision-2 reconstruction, puzzle and attempt identity, normal/Daily independence, restart/retry identity, Save/Load/View/Replay/Delete, recovery and deletion receipts, EN/ES/CA, themes, optional audio and offline operation. Classic/Daily must remain neutral about hidden-solution correctness. Android must use the same codecs and tests; this does not imply automatic device-to-device save synchronization.

Keep dependencies, size, CPU, memory and battery costs proportionate to measured benefits. Do not trade safe persistence for lower write counts or change generation identities for speed. No accounts, cloud sync, telemetry, ads, multiplayer, new modes or decorative feature work are needed for this roadmap.

## Minimal useful sequence

The planned sequence is 1.3.8 → 1.3.9 → 1.4.0. If evidence requires further work, proceed through 1.4.1 and subsequent normal patch releases, with 1.5.0 the latest acceptable Android-publicable milestone. Do not jump to 1.5.0 to conceal unfinished work; not every intervening patch number must be used. No dates or branches for future releases are created now.

| Release | Observable desktop benefit | Android progress | Scope, exclusions and dependency | Exit evidence |
|---|---|---|---|---|
| **1.3.8 — comfortable compact play** | Larger usable board in narrow windows; cancellable button clicks; Saved Games navigation without wheel/keyboard; reliable incremental rebuilds | Establish explicit UI action hit targets and press/release ownership on paths that already need them; improve portrait composition without claiming phone readiness | Execute the linked blueprint. No Android build, SDK, new codec, generator change or general platform abstraction. Starts at v1.3.7. | Core/UI/interaction/fit checks, compact and XL visual review, existing package matrix, unchanged data behavior; exact candidate acceptance under the development guidelines |
| **1.3.9 — direct touch and early device proof** | Direct touch on capable desktop hardware, safer gesture cancellation and usable Help/library scrolling; pointer and keyboard retained | First installable Android prototype from the shared engine under android/; prove resources, private storage, lifecycle, insets, Back and audio on a real arm64 phone before further architecture commitments | Depends on 1.3.8 interaction identities. Native touch adapter, duplicate-event policy, bounded list gestures; minimal host/build seams. Prototype may have explicitly listed secondary-screen limitations; no claim of parity or store readiness. | Desktop regression/package acceptance plus APK install and play/save/resume on real device, fixed-seed identity comparison, all prototype critical gates below. Do not postpone the first physical-device test to 1.4.0 |
| **1.4.0 — shared reliability and Android publicability target** | Robust interruption/resume behavior across screens, generation and audio; readable compact/HiDPI screens and scrolling improvements validated by the prototype | Complete functional parity and distribution candidate, including release APK/AAB preparation, native dependency audit and store-readiness material | Depends on recorded prototype findings. Close concrete shared lifecycle/UI/resource defects; no unrelated feature bundle. Publish Android here only when all publicability gates pass. | Full cross-platform regression, real-device parity, update/data preservation, native 16 KB/ABI validation, release-build/signing verification when separately authorized, honest store metadata and privacy/license material |
| **1.4.1 onward, ending no later than 1.5.0 — contingency only** | Only actual shared defects or usability gaps identified by testing | Close named publicability blockers; no second architecture | Not preapproved implementation scopes. At each release boundary identify the remaining blockers and update this plan. If remaining work is Android-only and supplies no desktop benefit, keep it within the ongoing useful release rather than inventing a filler desktop release. | Same publicability gates; if quality and the horizon truly conflict, report evidence and seek a decision before changing the horizon or dropping functionality |

Release 1.3.9 now has its executable blueprint linked above; 1.4.0 remains directional. Refine the latter from the early device evidence at its Phase 0 boundary. Earlier Android delivery is permitted only after satisfying the same gates. A successful v1.4.0 closes the Android-publicability objective; it does not force further releases.

## Milestones with different meanings

### Installable prototype — early in 1.3.9

Build an arm64-v8a debug APK in repository CI from a pinned commit. Use a distinct debug application identity/suffix so prototype data cannot be mistaken for release data. Install on a physical phone and record device/OS, commit, APK hash and observed results:

- Launch offline with bundled font, existing artwork and OGGs; optional audio failure remains nonfatal.
- Start Classic, select cells, enter/clear digits and notes, save and reopen the same attempt. Compare a fixed seed/difficulty/revision board with desktop.
- Home/background, screen lock and foreground return cannot consume paused Time Attack time, leave input pressed, render against a suspended surface or continue audible background playback.
- Background checkpoint plus process termination/relaunch restores acknowledged progress. Abrupt termination cannot promise an unsaved latest input; state the existing autosave window and verify recovery.
- Navigation bars/cutouts do not cover gameplay. Back closes the current overlay first, then returns through safe navigation without bypassing save gates.
- Rotate/resize and reopen without changing puzzle identity. Do not assume desktop window-size normalization is suitable for the Android window.
- Collect start latency, input latency, memory, idle/background CPU, package size and generation latency on the tested device. The first prototype is evidence, not an optimization contest.

Stop and address failures in these critical paths before adding optional screens or polishing store assets. Secondary limitations are permitted only if explicit and not mistaken for parity.

### Functional parity

Complete every mode/difficulty and Daily, all assistance/edit actions, Undo/Redo, settings, languages, themes, optional audio, Help/About, library exact/legacy views, Replay/Delete/recovery and save-error behavior without requiring a physical keyboard. Verify fixed-seed identity and files across the desktop and Android engines.

Exercise focus/manual/modal/Home/result pause ownership, generation cancellation, renderer reset, low-memory return, process recreation, audio-device changes, interrupted writes, full/unwritable storage, repeated save/load and date rollover. Preserve previous good copies and fail closed on future data. Android's process lifecycle must not depend on receiving desktop SDL_QUIT.

### Publishable Android

Parity is necessary but insufficient. Require:

- Reviewed release build from an exact source commit, stable app ID/versionCode policy, declared min/target SDK and supported ABIs, clean-install and upgrade tests, checksums and native dependency/license inventory.
- Signing and recoverable key custody handled through a separately authorized secure process; no keys or credentials in repository or continuity. A debug signature does not qualify.
- A distributable APK and, if Google Play is selected, a validated AAB and submission-ready listing. Store review/publication is a separate external event; do not describe a pending submission as approved.
- All bundled native libraries checked for 16 KB ELF/page compatibility and ZIP alignment, with execution in a 16 KB environment; not merely a compiler flag on the game's own library.
- Offline behavior with no unnecessary network/storage permissions, truthful data/privacy declarations, applicable distribution licensing, genuine localized screenshots and current support instructions. Store channel choice and public identity must be confirmed before irreversible registration.
- Real-device portrait and landscape/resize checks, system-bar/cutout safety, Android Back, meaningful touch targets, contrast, non-color-only cues, readable text and no blocked essential action.
- Accessibility limitations of an SDL-rendered surface documented honestly. Keyboard focus/contrast alone do not establish TalkBack support. Test TalkBack in the prototype; if useful semantic access requires an Android bridge, evaluate a bounded virtual-node bridge for controls/cells before publicability rather than claiming it already exists. Do not make a toolkit rewrite the default.
- Desktop release validation remains intact. No crash, data-loss, identity or critical interaction regression may be waived to meet the version number.

## Technical direction and decision points

### Host and build

Use SDL2's Android host/SDLActivity and a thin Android project referencing existing sources. The v1.3.9 blueprint selects Gradle plus a small CMake native target, supported by the inspected SDL2/add-on source. Retain ndk-build only as a bounded fallback if an actual integration failure demonstrates less maintenance. Do not migrate the desktop Makefile.

Pin compatible SDL2/ttf/mixer, NDK, AGP/Gradle/JDK and source checksums at that milestone. Compile dependencies from inspectable source where practical; no arbitrary prebuilt SDK bundle. Retain only the audio codecs actually needed for the four OGGs. Keep desktop dependency changes separate unless a demonstrated compatibility fix requires them.

v1.3.9 prototype decision: minSdk 28 and arm64-v8a; x86_64 only when needed for emulator validation. This supersedes the tentative minSdk26 hypothesis: inspected Android bionic declares the existing `getrandom` call from API28, avoiding an unnecessary entropy fallback. Final public support remains subject to device evidence. Do not weaken attempt-ID creation. Do not infer Android package identity from another application; `com.santiagorodriguez.sudokura` currently identifies the macOS bundle only.

### Input, viewport and lifecycle

Use one logical coordinate space for rendering and hit-testing. Translate touch once, track one active finger, cancel on screen/layout/lifecycle changes, and avoid processing both finger and synthetic mouse events. Keep mouse right-click and event-local keyboard modifiers intact. Android density-independent touch sizing is distinct from desktop logical pixels: a 36-pixel desktop control is not certified as a 48dp target.

Use the actual inset usable rectangle, density and current window dimensions. Keep desktop saved window bounds out of the Android fullscreen/resize path. Measure the board/control tradeoff on devices; do not shrink all controls to squeeze in desktop rows. The compact layout introduced in 1.3.8 is an incremental foundation, not the final mobile layout.

Handle application lifecycle before screen-specific handlers, including Settings, generation callbacks, startup scans and dialogs. Cancel transient presses, pause timing, checkpoint safely, suspend audio, stop rendering while backgrounded, then rebuild/revalidate resources on foreground. Resume gameplay only under the existing explicit pause semantics. Use existing save policy and atomic I/O, not a parallel Android save implementation.

### Resources and storage

The font discovery path currently looks for filesystem fonts; package a known redistributable font and load Android assets through SDL RWops-compatible APIs. Existing audio probing already uses SDL_RWFromFile; preserve its working desktop lookup order and test asset-relative Android paths. Keep generated artwork shared.

Use SDL's application-private preference location and the existing profile/library codecs. Confirm atomic rename, parent sync, locking and recovery on Android's private filesystem. No shared-storage permission or manual file picker is needed for initial gameplay. Cross-device import/export, cloud backup integration and synchronization are deferred; an Android OS backup policy must be explicit and tested before release so partial profile/library restoration cannot silently resurrect or lose records.

External URLs need an Android-aware opener (such as a bounded SDL_OpenURL path), retaining the allowlist; desktop fork/xdg-open is not an Android strategy.

## Research used for concrete choices

Official references checked on 2026-10-26; revalidate mutable requirements before the Android launch:

- [SDL2 Android integration](https://wiki.libsdl.org/SDL2/README-android): SDLActivity/Gradle host, assets and lifecycle handling.
- [SDL2 touch-to-mouse events](https://wiki.libsdl.org/SDL2/SDL_HINT_TOUCH_MOUSE_EVENTS): synthetic mouse events are enabled by default; missing FINGER handlers alone does not prove touch cannot click.
- [Android 16 KB compatibility](https://developer.android.com/guide/practices/page-sizes): all native dependencies and packaging need validation.
- [Google Play target API requirements](https://support.google.com/googleplay/android-developer/answer/11926878?hl=en): current phone submissions require API 36; this is targetSdk, not minSdk, and must be checked again at launch.
- [Edge-to-edge and insets](https://developer.android.com/develop/ui/views/layout/edge-to-edge): current targets require explicit treatment of system bars.
- [Android accessibility](https://developer.android.com/guide/topics/ui/accessibility/apps): 48dp touch-target recommendation, contrast and semantic accessibility inform device acceptance.

These references support retaining SDL2 with small host seams. They do not prove that the current Sudokura binary runs on Android.

## Verified release progress and next gate

v1.3.8 is published at the current baseline above. PR #14 merged its compact controls, More panel, pointer cancellation and Saved Games paging. Exact-head CI runs 40406632381/40397978066 and release workflow 40406631994 completed successfully. The public release contains the established desktop artifacts. These observations do not certify Android, physical touch, or independent Windows/macOS hardware acceptance.

v1.3.9 Phase 0 selects native single-owner touch, bounded Help/library drag also usable by mouse, and a source-built arm64 debug host. The release blueprint owns precise behavior, build versions, prototype-only identity/backup policy, density/inset geometry, lifecycle/Back/resource work and acceptance. No Android implementation or device success is claimed yet. The first physical-device gate is after the critical play/save/resume slice, before optional mobile completion; emulator/CI alone cannot pass it.

A narrow phone cannot fit nine 48dp cells across its width. The prototype must measure and disclose board-cell size/comfort while giving action controls meaningful targets; this is not a blanket accessibility claim. Any paused rotate/enlarge fallback for short surfaces is prototype-only and must be resolved before functional parity/publicability. TalkBack remains an evidence-driven decision before publicability, not an assumed capability or automatic rewrite mandate.

Public Android identity, final minimum support, distribution channels, signing custody and public backup/transfer policy remain decisions before launch. The prototype's separate temporary identity and disabled backup do not preempt them. The v1.4.0 target and v1.5.0 maximum horizon are unchanged.

## Maintenance of this plan

Update only at a meaningful decision, verified prototype, release closure or changed dependency/blocker. Keep release implementation details in their release blueprint; retain this document as the owner of the horizon and milestone gates. Never mark the roadmap complete merely because Phase 0 or v1.3.8 is complete. If a future release promotes this plan to another canonical path/ref, verify that the full current material is retrievable before retiring the old owner.
