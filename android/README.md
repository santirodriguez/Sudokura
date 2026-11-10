# Sudokura Android build

The familiar Sudoku grid, now made for thumbs. Sudokura 1.4.0 brings Classic, dated Daily challenges and Your journey to Android. This page is for people building or testing the app; for a quick introduction, start with the [main README](../README.md).

**Availability and compatibility:** 1.4.0 is the first public Android distribution version after private debug prototypes. Its signed production candidate passed maintainer phone testing, but a passing build doesn't mean an APK is published: check [GitHub Releases](https://github.com/santirodriguez/Sudokura/releases). Android 9+ ARM64 is required. Google Play and F-Droid are separate channels. For signing and safe updates, see [Android packaging](RELEASING.md); specialized unverified behavior is documented below.

## Pinned shared-engine host

- C11/SDL2 shared gameplay and persistence; the desktop Makefile remains unchanged.
- Java 17, Gradle 8.13, AGP 8.11.1; compile/target SDK 36, minSdk 28.
- Build Tools 35.0.0, NDK 28.2.13676358, CMake 3.22.1; arm64-v8a only.
- SDL2 2.32.10, SDL2_ttf 2.24.0 with vendored FreeType/no HarfBuzz, SDL2_mixer 2.8.1 with STB Vorbis/WAV.
- Exact source URLs/digests: [dependencies.lock.json](dependencies.lock.json). SDL Java/native sources use the same verified archive.
- No INTERNET or broad storage permission; cloud backup/device transfer excluded. OEM behavior requires device evidence.
- The canonical font, OGGs, icons and license notices are bundled and audited. Audio failure remains nonfatal.

## Variants and CI

`prototypeDebug` preserves `org.sudokura.prototype.debug`, the Prototype label and the version-derived `-prototype` suffix. `productionRelease` uses confirmed `org.cajapersonal.sudokura`, label Sudokura and the plain version from `src/version.h` (published v1.4.0 / code 1; v1.4.1 candidate / code 2). Gradle emits unsigned production APK/AAB packages. Only a separately authorized manual `release.yml` job can sign and verify a direct APK with the original app key; ordinary CI never signs. The Play AAB remains unsigned staging output. The other flavor/build combinations are disabled.

The [Android package workflow](../.github/workflows/android-preview.yml) is reusable from [CI](../.github/workflows/ci.yml) and supports manual dispatch. Its `android-debug` job validates inputs, generates shared assets, builds the prototype and unsigned production APK/AAB, audits identities/assets/native libraries/alignment, and retains unsigned Actions artifacts **without app-signing secrets**. The separate manual-only `release.yml` workflow verifies an approved SHA and unsigned receipts, then uses the existing repository Secrets only in its `android-sign` step after checks. It requires no Environment or secret migration. Repository Secrets are not isolated from other privileged repository workflows, so manual source/actor checks do not replace GitHub permissions. Ordinary CI's `phase9-release-bundle` contains four desktop applications; an authorized signed release workflow produces a separate five-application `release-bundle`. Tag pushes are not automatic signing or draft-release triggers in this candidate. The AAB passes official bundletool validation; Console acceptance remains separate. These are internal candidates, not a public release.

For an already equipped environment (no local tool installation requested):

```sh
python3 android/scripts/prepare_sources.py
make assets
bash android/gradlew --no-daemon :app:assemblePrototypeDebug :app:assembleProductionRelease :app:bundleProductionRelease -PsudokuraFont=/verified/path/DejaVuSans.ttf -PsudokuraFontLicense=/verified/path/font-copyright
```

The launcher verifies/extracts pinned Gradle; it is not a committed wrapper JAR. Downloaded inputs, generated C/artwork, SDKs and build outputs are ignored. Production signing and Play upload procedures are separate from Gradle compilation.

## Runtime evidence and limits

Portrait targets 360x600 usable dp and larger; supported landscape starts at 600x320dp. Shared render/input geometry preserves 48dp action controls and a square board at least 270dp at the minimum. Board cells are necessarily smaller than 48dp on narrow phones. Smaller surfaces preserve the attempt behind a paused enlarge-or-rotate fallback. Android insets are applied once, and a bounded post-rotation repaint lets initial Home surfaces settle.

The maintainer accepted gameplay, More styling, official/localized links and initial-Home rotation on the recorded debug APKs. The semantic bridge publishes visible controls and cells through bounded snapshots; Java focus does not edit the game and native actions reject stale context. Host automated tests and successful Android compilation do not establish a complete physical TalkBack journey.

The maintainer accepted the signed production candidate, revised journey/dialog design, Try again, and explicit Save followed by Android force stop/reopening with numbers and notes retained. Full TalkBack navigation, ARM64 fixture equivalence, the exhaustive lifecycle matrix, measured performance, 16 KiB runtime compatibility and independent reproducibility remain unverified. The November 4 [release acceptance amendment](../docs/V1.4.0_BLUEPRINT.md#release-acceptance-amendment--november-4-2026) defers extended manual validation and further emulator debugging from the immediate release-preparation path. Structural alignment is not runtime acceptance. Stable upgrade and channel switching remain unverified; the approved screenshot provenance and final pre-merge public-copy review are recorded in the [v1.4.0 blueprint](../docs/V1.4.0_BLUEPRINT.md).

Keep an existing prototype installation with progress: an ephemeral debug-signature mismatch is not a reason to uninstall or clear data. Production uses separate storage and does not promise automatic prototype migration. GitHub Releases/Obtainium is the primary direct distribution channel when a signed APK is published; F-Droid inclusion and Play submission/approval are external work and are not implied by GitHub publication.


## Opt-in 16 KiB emulator diagnostic

A push explicitly marked `[android-runtime]` enables one diagnostic job inside the existing CI run. It builds only the x86_64 prototype with `-PsudokuraEmulatorTest=true`; the production variant is disabled in that configuration and normal builds remain ARM64. No signing secrets are available to the diagnostic job.

The job uses the SDK's API35 `google_apis_ps16k` image, records its revision, requires `getconf PAGE_SIZE` to report16384 and disables page-size compatibility fallback. It checks native ELF/ZIP alignment, loads all four shared libraries, exercises Home/play/background/explicit Resume/landscape through the actual Android surface and compares full deterministic core fixtures against the host executable. Available XML/PNG/logs are retained even on failure; the source/APK-bound runtime receipt is emitted only when the full runtime path succeeds. This is an x86_64 emulator diagnostic, not acceptance of the public ARM64 APK, TalkBack, or physical-device performance. Official setup: https://developer.android.com/guide/practices/page-sizes .
