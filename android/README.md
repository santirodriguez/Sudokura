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

## Prototype status and limitations

The arm64 debug host, bundled resources, app-private Save/recovery, touch dispatch, Android Back/lifecycle seams and mobile layout have been implemented and compiled in the Android preview workflow. The native libraries and APK packaging passed structural 16 KiB alignment audits; execution on an actual 16 KiB device is **not verified**.

A real-phone functional test round was completed with positive maintainer feedback, including gameplay/music, refreshed mobile controls, and explicit Save followed by closing/reopening the app with entered numbers, notes and progress retained. This is a user-reported functional result, not a measured or itemized test report. The exact installed APK digest, device/API/density/usable dp, fixed-seed desktop/Android equality, forced OS-process-kill recovery, full Back/rotation/cutout/Time Attack matrix, input/cold-start/CPU/memory measurements, TalkBack observations and 16 KiB runtime execution were not independently recorded. These remain prototype evidence gaps and must not be presented as completed publicability tests.

The compact portrait layout targets at least 360x600dp of usable surface, with a square board and 48dp main action controls; individual board cells are necessarily smaller than 48dp on narrow phones. Too-short rotated screens conceal and pause gameplay with an instruction to return to portrait. Landscape parity, a screen-reader semantic bridge, public signing/upgrade support, cross-device transfer and store distribution are **not** implemented or accepted.

The prototype has a separate application ID and debug signing that can differ across CI runs. Android may refuse an update signed with a different key. Do not uninstall or clear an installation carrying saved progress simply to try another preview. Android backup and transfer are intentionally disabled for the prototype. Keep the existing Saved Games and recovery safeguards; do not use production user data for destructive recovery testing.

Do not add this APK to the six usual Sudokura public release assets without separate authorization. v1.4.0 is the preferred Android publishability milestone and v1.5.0 the maximum horizon; public ID, backup policy, supported devices, distribution channels and signing custody remain future explicit decisions.

Full requirements live in [the v1.3.9 blueprint](../docs/V1.3.9_BLUEPRINT.md) and [Android release roadmap](../docs/ANDROID_RELEASE_PLAN.md).
