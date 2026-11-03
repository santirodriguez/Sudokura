# First public Android release: Sudokura 1.4.0

Status: preparation, not publication. Sudokura 1.4.0 is planned as the **first public Android release**. The earlier Android APKs were private debug prototypes. Keep the shared project version `1.4.0` and Android versionCode `10400`; “first Android release” is release/listing copy, not a second version sequence or a reset to 1.0.

## One production identity

| Variant | Application ID | Version | Intended use |
| --- | --- | --- | --- |
| Prototype debug | `org.sudokura.prototype.debug` | `1.4.0-prototype` / 10400 | Separate test installation; ephemeral debug signer |
| Production release | `org.cajapersonal.sudokura` | `1.4.0` / 10400 | Unsigned preparation, then separately authorized stable signing |

The production ID was confirmed on November 3, 2026. The internal Java namespace remains independent of the application ID; retain the JNI class names. The production label is Sudokura. Prototype data is separate; no automatic migration or uninstall workaround is promised. Keep backup/device transfer excluded and preserve local progress.

Version components come from `src/version.h`, using major*10000+minor*100+patch with positive signed-32-bit bounds. Codes must increase for every subsequently distributed production build, including prereleases; never replace distributed bytes under the same version/code. Before an actual store upload, verify the highest consumed code in that store. Initial F-Droid and Play listings must identify this as the first Android release. Neither store is currently claimed available or approved.

## Unsigned preparation

The existing reusable Android job, called by CI, builds only `prototypeDebug` and `productionRelease`. It retains:

- `sudokura-android-debug-prototype`: the installable debug APK and structural receipts.
- `sudokura-android-unsigned-production`: the unsigned production APK, unsigned Play AAB, source/version/checksum receipts and APK/native/alignment audits.

The production APK is not installable until signed. AAB container checks are preliminary: official bundletool validation, upload signing and Console acceptance remain open. This preparation does not request secrets, create keys, use production signing, register an app, upload a bundle, create a tag or publish a release. Native 16 KiB alignment is not 16 KiB runtime acceptance.

From an already equipped environment, build the pinned sources/assets and run:

```sh
bash android/gradlew --no-daemon :app:assemblePrototypeDebug :app:assembleProductionRelease :app:bundleProductionRelease -PsudokuraFont=/verified/path/DejaVuSans.ttf -PsudokuraFontLicense=/verified/path/font-copyright
python3 android/scripts/audit_apk.py /path/to/aapt android/app/build/outputs/apk/production/release/app-production-release-unsigned.apk --variant release --require-unsigned
```

Do not install tooling on the maintainer's computer merely to reproduce CI. Final signed APK naming remains `Sudokura-1.4.0-Android-arm64.apk`; files containing `unsigned` are staging artifacts, not public downloads. AABs remain internal Play preparation, not GitHub public release assets. Desktop release assets and their existing checks remain unchanged.

## Signing and recovery plan

Use **new, independent Sudokura keys**, following the same separation of roles used by the other applications; never reuse another application's private keys or certificates.

| Role | Planned private filename / alias | Purpose | Current state |
| --- | --- | --- | --- |
| Long-lived app signing | `sudokura-release.jks` / `sudokura` | Signs GitHub/Obtainium APKs; intended common installed-app identity across channels | Not created or enrolled |
| Dedicated Play upload | `sudokura-play-upload.jks` / `sudokura-play-upload` | Signs AAB uploads; not the direct APK signing identity | Not created or enrolled |
| PEPK transfer archive | Provider-generated encrypted transfer plus original public certificate | Only if separately enrolling the original app key in Play App Signing | Not generated |

Before key creation/signing, agree the secure operator procedure and recoverable custody. Keep private material outside the repository and chat. Use labelled private groups `01-app-signing/`, `02-play-upload/`, and, when applicable, `03-play-transfer/`. Keep passwords separately protected. Verify an independent encrypted/offline recovery copy and match the restored public certificate fingerprint; a second copy on the same disk is not independent recovery.

Record public certificate SHA-256 fingerprints and non-secret package/source receipts after actual key creation. A filename or alias does not establish signing identity. Never silently regenerate a missing keystore: restore the exact backup and verify its certificate. Do not put JKS, passwords, base64 keystores or private transfer material into source, Issues, logs or ordinary artifacts. No Actions secrets have been created/configured by this preparation.

For Play, propose enrolling the original app-signing identity and using the separate upload key, so Play does not silently introduce a different installed-app signer. Actual enrollment and private-key transfer need separate authorization. Verify the live Console certificate before relying on enrollment. F-Droid use of the developer signer requires an accepted reproducible-build recipe and signature verification; it is not automatic. A different store signer can block in-place cross-store updates. Do not promise channel switching until tested with retained data.

Pin and validate the signing toolchain against the F-Droid signature-copy/rebuild process before signing the first public APK. Preserve the same app certificate for future direct updates. Do not select a different compiler/NDK merely to copy another application's signing configuration.

## Before public distribution

The full [blueprint](../docs/V1.4.0_BLUEPRINT.md) and [roadmap](../docs/ANDROID_RELEASE_PLAN.md) retain the physical lifecycle/storage, ARM64 parity, 16 KiB runtime, accessibility, performance, stable upgrade/Obtainium and screenshot/public-copy gates. Independently rebuild the same exact unsigned source and compare bytes before claiming reproducibility. Record same-ID/same-key clean-install and sequential-upgrade evidence with retained saves. Store submissions and acceptance are external steps.

Review the exact signed artifacts, public certificates, immutable source, checksums, genuine screenshots and final EN/ES/CA copy before separately authorized merge/publication. First-Android release wording does not waive those gates.
