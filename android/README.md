# Sudokura Android build

Sudokura 1.4.0 is planned as the **first public Android release**, following the earlier private debug prototypes. The signed production candidate has been accepted in maintainer phone testing. Publication and store availability remain pending; specialized unverified behavior is listed below. See [release and signing preparation](RELEASING.md) for the exact identities, custody plan and distribution gates.

## Pinned shared-engine host

- C11/SDL2 shared gameplay and persistence; the desktop Makefile remains unchanged.
- Java 17, Gradle 8.13, AGP 8.11.1; compile/target SDK 36, minSdk 28.
- Build Tools 35.0.0, NDK 28.2.13676358, CMake 3.22.1; arm64-v8a only.
- SDL2 2.32.10, SDL2_ttf 2.24.0 with vendored FreeType/no HarfBuzz, SDL2_mixer 2.8.1 with STB Vorbis/WAV.
- Exact source URLs/digests: [dependencies.lock.json](dependencies.lock.json). SDL Java/native sources use the same verified archive.
- No INTERNET or broad storage permission; cloud backup/device transfer excluded. OEM behavior requires device evidence.
- The canonical font, OGGs, icons and license notices are bundled and audited. Audio failure remains nonfatal.

## Variants and CI

`prototypeDebug` preserves `org.sudokura.prototype.debug`, the Prototype label and the version-derived `-prototype` suffix. `productionRelease` uses confirmed `org.cajapersonal.sudokura`, label Sudokura and plain source version, currently 1.4.0 / 1. Gradle emits unsigned production packages. The trusted release-branch CI subsequently signs and verifies the direct APK with the existing app key; it never generates a long-lived key. The Play AAB remains unsigned staging output. The other flavor/build combinations are disabled.

The [Android package workflow](../.github/workflows/android-preview.yml) is one reusable job within [CI](../.github/workflows/ci.yml), with optional manual dispatch. It verifies inputs, generates shared assets, builds the prototype and unsigned production APK/AAB, audits both APK identities/assets/native closure/alignment, and retains separate Actions artifacts, including `sudokura-android-signed-github` on the trusted release branch. The AAB passes official bundletool validation; Console acceptance remains separate. These are internal candidates, not a public release.

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

The maintainer accepted the signed production candidate, revised journey/dialog design, Try again, and explicit Save followed by Android force stop/reopening with numbers and notes retained. Full TalkBack navigation, ARM64 fixture equivalence, the exhaustive lifecycle matrix, measured performance, 16 KiB runtime compatibility and independent reproducibility remain unverified. The November 4 [release acceptance amendment](../docs/V1.4.0_BLUEPRINT.md#release-acceptance-amendment--november-4-2026) defers extended manual validation and further emulator debugging from immediate release preparation. Structural alignment is not runtime acceptance. Stable upgrade/channel switching and final screenshot/public-copy review remain separate.

Keep an existing prototype installation with progress: an ephemeral debug-signature mismatch is not a reason to uninstall or clear data. Production uses separate storage and does not promise automatic prototype migration. GitHub/Obtainium is the primary planned direct channel; F-Droid inclusion and Play submission/approval remain external work, not currently available listings.


## Opt-in 16 KiB emulator diagnostic

A push explicitly marked `[android-runtime]` enables one diagnostic job inside the existing CI run. It builds only the x86_64 prototype with `-PsudokuraEmulatorTest=true`; the production variant is disabled in that configuration and normal builds remain ARM64. No signing secrets are available to the diagnostic job.

The job uses the SDK's API35 `google_apis_ps16k` image, records its revision, requires `getconf PAGE_SIZE` to report16384 and disables page-size compatibility fallback. It checks native ELF/ZIP alignment, loads all four shared libraries, exercises Home/play/background/explicit Resume/landscape through the actual Android surface and compares full deterministic core fixtures against the host executable. Available XML/PNG/logs are retained even on failure; the source/APK-bound runtime receipt is emitted only when the full runtime path succeeds. This is an x86_64 emulator diagnostic, not acceptance of the public ARM64 APK, TalkBack, or physical-device performance. Official setup: https://developer.android.com/guide/practices/page-sizes .
