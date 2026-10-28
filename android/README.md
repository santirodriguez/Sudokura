# Sudokura Android prototype (v1.3.9)

This is an isolated, **debug-only** host for the existing C11/SDL2 game. It is not Android functional parity, public launch approval, or a release artifact. Keep the desktop Makefile and all desktop release assets unchanged.

## Build selection

- Java 17, Gradle 8.13, Android Gradle Plugin 8.11.1.
- compileSdk and targetSdk 36; minSdk 28; Build Tools 35.0.0.
- NDK r28c (28.2.13676358), CMake 3.22.1; only arm64-v8a.
- SDL2 2.32.10, SDL2_ttf 2.24.0 (vendored FreeType, no HarfBuzz), SDL2_mixer 2.8.1 (STB Vorbis and WAV only).
- Exact archive URLs and SHA-256 digests: [dependencies.lock.json](dependencies.lock.json).
- Temporary application ID: `org.sudokura.prototype.debug`. VersionName and versionCode derive from `src/version.h`; currently `1.3.9-prototype` / `10309`.
- No INTERNET or broad storage permissions; application backup and device transfer excluded. OEM behavior still requires verification on an actual phone.
- Runtime OGGs, DejaVuSans.ttf and source/font license notices are APK assets. The four OGGs resolve as `audio/*.ogg` relative to the APK's asset root; the audit verifies bytes against the canonical OGG masters. The font must be a legitimate installed redistributable copy; never commit a downloaded binary merely to satisfy the host.
- The launcher icon is generated at build time from `assets/branding/source/sudokura-512.png`, with a native Android adaptive mask and no new branding master. The prototype theme removes the default Android ActionBar and uses dark system chrome, independently of the saved SDL theme setting.

## Clean CI build

The [Android preview workflow](../.github/workflows/android-preview.yml) prepares a clean Ubuntu runner, verifies source/Gradle downloads **before** unpacking them, generates shared art using the existing Go script, assembles the debug APK, audits native/manifest/assets and attaches the APK and checksum/metadata receipt as *Actions artifacts only*. No release/tag, signing key, store, or package publication is created. The debug APK uses the runner's ephemeral standard debug identity; not a stable signing identity.

For an already equipped Android development environment (no local installation is requested by this project):

1. From repository root: `python3 android/scripts/prepare_sources.py`.
2. Run `make assets` to regenerate ignored C branding/flags using the existing Go generator.
3. With Java 17 and Android SDK/NDK/CMake versions above already installed:
   `sh android/gradlew --no-daemon :app:assembleDebug -PsudokuraFont=/path/to/DejaVuSans.ttf -PsudokuraFontLicense=/path/to/dejavu-copyright`.
4. Validate with `python3 android/scripts/audit_apk.py /path/to/aapt android/app/build/outputs/apk/debug/app-debug.apk` and `sha256sum` of that exact file.
5. Install only a concrete verified debug APK on a supported arm64 phone once the critical gameplay/lifecycle/storage integration is ready. Do not uninstall the only copy of progress to work around a different CI debug signing key.

The committed `android/gradlew` is a small **shell launcher**, not a Gradle wrapper JAR. It verifies and extracts the pinned official Gradle distribution; invoke with `sh` if its file mode is non-executable. Downloaded source archives, SDKs, Gradle distribution/build trees, and generated C artifacts are ignored, not versioned. Java SDLActivity sources come from the exact same verified SDL tarball as native SDL.

On the minimum usable 360x600dp portrait viewport, gameplay uses a >=270dp board, 48dp toolbar/keyboard controls, readable HUD chips and a progress row shared with brief notifications. On too-short rotated screens, the prototype hides the board and focus-pauses the attempt with a localized return-to-portrait instruction; Android Back retains its checked navigation. This is a prototype-only limitation, not landscape parity.

## Gating and known limitations

This file establishes the unit-1 Android **build vertical slice**, not a verified runtime. Until unit 2, Android-specific lifecycle/save-on-background, touch slop from ViewConfiguration, font load path, density/insets, Back, URL opening and mobile layout remain implementation gates. Do not infer functional quality from a successful assembleDebug or emulator.

Unit 3 mandates a **physical arm64 phone** immediately after critical play/save/resume integration: offline start, play/Notes/Save/reopen same attempt, fixed-seed identity comparison, foreground/background/lock, acknowledged checkpoint and process relaunch, Back, rotate/cutout, audio optional, measurements and TalkBack observations. Record device, OS/API/ABI/density/usable dp, exact source SHA/APK hash, and pass/fail for each critical path. CI/emulator never substitute for this. A debug signature cannot pass public signing/upgrade gates; prototype status remains explicit.

Do not add this APK to the six usual Sudokura public release assets without separate authorization. v1.4.0 is the preferred Android publishability milestone and v1.5.0 the maximum horizon; public ID, backup policy, supported devices, distribution channels and signing custody remain future explicit decisions.

Full requirements live in [the v1.3.9 blueprint](../docs/V1.3.9_BLUEPRINT.md) and [Android release roadmap](../docs/ANDROID_RELEASE_PLAN.md).
