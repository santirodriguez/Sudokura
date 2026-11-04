# First public Android release: Sudokura 1.4.0

This is the packaging and signing record for Sudokura 1.4.0, the **first public Android distribution version** following private debug prototypes. Live APK availability is determined by [GitHub Releases](https://github.com/santirodriguez/Sudokura/releases), not by the state of this document. The shared product version is `1.4.0` and the initial Android versionCode is `1`; “first Android release” is listing copy, not a second version sequence or reset to 1.0.

## One production identity

| Variant | Application ID | Version | Intended use |
| --- | --- | --- | --- |
| Prototype debug | `org.sudokura.prototype.debug` | `1.4.0-prototype` / 1 | Separate test installation; ephemeral debug signer |
| Production release | `org.cajapersonal.sudokura` | `1.4.0` / 1 | Gradle stages unsigned APK/AAB; trusted release-branch CI signs the direct APK |

The production ID was confirmed on November 3, 2026. The internal Java namespace remains independent of the application ID; retain the JNI class names. The production label is Sudokura. Prototype data is separate; no automatic migration or uninstall workaround is promised. Keep backup/device transfer excluded and preserve local progress.

The visible version comes from the MAJOR/MINOR/PATCH components in `src/version.h`. The independent `SUDOKURA_VERSION_ANDROID_CODE` in the same header starts at 1 and is shared by GitHub/Obtainium, F-Droid and Google Play for equivalent releases. It must stay between 1 and 2100000000. Codes must increase for every subsequently distributed production build, including prereleases; never replace distributed bytes under the same version/code. Before an actual store upload, verify the highest consumed code in that store. Initial F-Droid and Play listings must identify this as the first Android release. Internal production candidates used code 10400; Android refuses a normal downgrade to code 1. Preserve installations holding progress: no uninstall/data-clear workaround or automatic progress migration is provided. Matching version numbers alone do not establish cross-store signing compatibility. F-Droid and Play acceptance must be checked independently; GitHub publication does not establish their availability.

## Build outputs and verified signing

The existing reusable Android job, called by CI, builds only `prototypeDebug` and `productionRelease`. It retains:

- `sudokura-android-debug-prototype`: the installable debug APK and structural receipts.
- `sudokura-android-unsigned-production`: the unsigned production APK, unsigned Play AAB, source/version/checksum receipts and APK/native/alignment audits.
- `sudokura-android-signed-github`: the signed direct APK, verified app certificate and source-bound manifest/checksum receipts.
- `phase9-release-bundle`: the five application files with shared Build-Info and checksums, after the standard cross-platform package jobs succeed.

The production APK is not installable until signed. CI validates the AAB with official bundletool 1.18.3, downloaded from its release with SHA-256 verification, and retains the validation output and tool identity. This structural validation does not establish upload signing or Console acceptance; both remain open. Repository secrets `SUDOKURA_APP_KEYSTORE_BASE64` and `SUDOKURA_APP_KEYSTORE_PASSWORD` let the trusted release-branch job sign an internal GitHub APK artifact. Pull-request runs never receive or use them. The Play upload JKS stays separate and is not used by this APK-signing job. A signed CI artifact is not a public release. Native 16 KiB alignment is not 16 KiB runtime acceptance.

From an already equipped environment, build the pinned sources/assets and run:

```sh
bash android/gradlew --no-daemon :app:assemblePrototypeDebug :app:assembleProductionRelease :app:bundleProductionRelease -PsudokuraFont=/verified/path/DejaVuSans.ttf -PsudokuraFontLicense=/verified/path/font-copyright
python3 android/scripts/audit_apk.py /path/to/aapt android/app/build/outputs/apk/production/release/app-production-release-unsigned.apk --variant release --require-unsigned
```

Do not install tooling on the maintainer's computer merely to reproduce CI. Final signed APK naming remains `Sudokura-1.4.0-Android-arm64.apk`; files containing `unsigned` are staging artifacts, not public downloads. AABs remain internal Play preparation, not GitHub public release assets. Desktop release assets and their existing checks remain unchanged.

## Existing signing identities and recovery

The two independent Sudokura keys have already been created. The app key signs the direct APK in CI; the Play upload key remains separate. Do not generate replacements or repeat initial setup for a normal build, update or password change. Never reuse another application's keys.

| Role | Private filename / alias | Purpose | Current state |
| --- | --- | --- | --- |
| Long-lived app signing | `sudokura-release.jks` / `sudokura` | Signs GitHub/Obtainium APKs; intended common installed-app identity across channels | Created locally; app-signing certificate verified |
| Dedicated Play upload | `sudokura-play-upload.jks` / `sudokura-play-upload` | Signs AAB uploads; not the direct APK signing identity | Created locally; upload certificate verified |
| PEPK transfer archive | Provider-generated encrypted transfer plus original public certificate | Only if separately enrolling the original app key in Play App Signing | Not generated |

### Public certificate identities

| Role | Certificate SHA-256 |
| --- | --- |
| App signing | `FD:E9:51:E3:D2:EF:42:E4:1C:71:86:EE:96:59:44:AE:CA:54:9D:59:0A:D0:80:71:1D:D3:D8:75:F8:2D:F6:6D` |
| Play upload | `ED:32:FB:EF:2E:28:31:92:E8:0F:74:E1:11:28:82:37:71:3C:61:6E:74:72:8B:56:79:3E:2A:C3:CE:BE:A4:86` |

These identify the public certificates, not JKS-file checksums. A matching alias or filename alone does not establish the signer.

### Current CI configuration

Exactly two repository secrets are used for direct APK signing:

- `SUDOKURA_APP_KEYSTORE_BASE64`: base64 of the app-signing JKS.
- `SUDOKURA_APP_KEYSTORE_PASSWORD`: the shared JKS store/key-entry password.

The alias is fixed as `sudokura`. The store password and key-entry password are different concepts, but this key uses the same value for both; no third secret is needed. Signing is allowed on the exact trusted `release/1.4.0` branch source and on push events for version-matched release tags whose exact commit belongs to `main`. Pull requests, arbitrary refs, mismatched sources and manual tag dispatch do not sign. The context is checked before the signing step receives credentials. The Play upload JKS and password are not used by this workflow.

Keep private JKS files and password records outside source control, chat, Issues and ordinary build artifacts. Base64 is an encoding, not encryption. Do not echo secret values or enable shell tracing while handling them. Keep JKS files mode 0600 and their private directory mode 0700. Preserve role-labelled `01-app-signing/`, `02-play-upload/` and `public/` groups in private custody; only exported public certificates belong in the last group.

Changing a JKS password does not by itself create a new app-signing certificate. It also does not invalidate a previously copied keystore protected by the old password. For subsequent CI signing, update both the uploaded JKS content and its matching password secret if they changed. Keep any store/key-entry passwords consistent with the workflow, or deliberately adapt the configuration. Do not rotate the app's signing identity as a password-change workaround. Removing CI secrets prevents future CI signing but does not revoke earlier signed APKs or copies of the private key; local signing remains possible with the original key.

### Backup and recovery

The maintainer reports that both JKS files are backed up. An independent restore exercise has not been verified. Keep encrypted/offline recovery copies and separately protected password records; a second copy on the same disk is not independent recovery.

Restore copies into an isolated private directory without overwriting the originals. Open each restored JKS interactively with its recorded store password and alias, and compare the exported public certificate fingerprint with the table above. Listing a certificate validates access to the keystore, not the ability to use a separately password-protected private key: verify the key-entry password through an isolated signing check when exercising full recovery. Record recovery as verified only after the appropriate check succeeds.

If a key file or password is missing, recover the exact original rather than generating a replacement. Never uninstall a progress-bearing app to hide a signing mismatch. Preserve the long-lived app certificate for future direct updates.

For Play, propose enrolling the original app-signing identity and using the separate upload key, so Play does not silently introduce a different installed-app signer. Actual enrollment and private-key transfer need separate authorization. Verify the live Console certificate before relying on enrollment. F-Droid use of the developer signer requires an accepted reproducible-build recipe and signature verification; it is not automatic. A different store signer can block in-place cross-store updates. Do not promise channel switching until tested with retained data.

The direct APK uses the pinned signing toolchain. Before claiming F-Droid developer-signature/reproducible-build support, validate that toolchain against the applicable signature-copy/rebuild process. Preserve the same app certificate for future direct updates. Do not select a different compiler/NDK merely to copy another application's signing configuration.

## Internal combined candidate

The trusted release-branch CI signs and verifies the APK, then emits `artifact-manifest-android.json`, `SHA256SUMS-android.txt` and `build-provenance-android.txt`. These bind source/version, application ID, app certificate, APK bytes and signed audit receipts. They do not certify device, reproducibility, store or 16 KiB runtime acceptance.

When that same CI run builds Android, its final internal bundle includes the signed APK alongside the four desktop applications, shared `SHA256SUMS.txt` and `Sudokura-<version>-Build-Info.json`. `scripts/release_bundle.py --help` exposes an explicit `--include-android` option on each subcommand; the desktop-only contract remains available when Android is not built. Missing/mismatched Android bytes, source, identity or signer fail validation.

### Exact public upload handoff

For a pre-publication audit or separately authorized manual upload, use an accepted `phase9-release-bundle` from one exact release-branch CI run. The bundle contains all five application packages and their shared Build-Info/checksums. Set `SOURCE_COMMIT` to that specific candidate's complete 40-character Git commit SHA. The export command rechecks every file against the embedded per-platform manifests, exact source commit, version and Android signer, then stages only seven public files in a new directory:

```sh
python3 scripts/release_bundle.py export \
  --artifacts-dir /path/to/extracted/phase9-release-bundle \
  --output-dir /path/to/new/public-upload \
  --version 1.4.0 --source-commit "$SOURCE_COMMIT" --include-android
```

This stages files only: it creates no tag, draft, release or network request, and refuses an existing output directory. Do not substitute unsigned APKs, AABs, per-platform receipts or another run's checksums. Keep the downloaded Actions ZIP digest and source commit with the acceptance record.

For normal publication, separately authorize the exact `main`-ancestor source commit and release tag. The tag-triggered workflow rebuilds and signs Android and desktop from its resolved source, validates seven release assets, and creates a draft only; do not relabel older branch-CI bytes as a new tag build. Validate the draft's seven uploaded assets using `scripts/release_bundle.py validate-assets --version 1.4.0 --assets-json draft-release-assets.json --include-android` and independently confirm the uploaded bytes, source and checksums before publication; after publication require the `--published` validation mode. The export procedure above is an audit/staging option, not a requirement to replace the tag-built draft. Any source change requires regeneration and validation of its own exact-source bundle.

The tag workflow now calls the reusable Android job with the resolved source commit and requires trusted signing. Its combined bundle includes the signed APK, and the draft upload, labels and asset validator all require seven files. A failure in Android, either Linux runtime check or another package job blocks the draft. Tags trigger the release workflow without also triggering the general CI. Manual packaging creates no draft and must use an exact trusted workflow source; it cannot sign an arbitrary historical ref. No tag or draft is created as part of a branch CI run.

## Distribution acceptance and limitations

The November 4 acceptance amendment in the [blueprint](../docs/V1.4.0_BLUEPRINT.md) and [roadmap](../docs/ANDROID_RELEASE_PLAN.md) defers full manual accessibility, extended performance measurements and further emulator debugging. Preserve their unverified status and public limitations; known critical defects, source/signing/checksum integrity, and screenshot/public-copy review remain release requirements. Independently rebuild the same exact unsigned source and compare bytes before claiming reproducibility. Record same-ID/same-key clean-install and sequential-upgrade evidence with retained saves. Store submissions and acceptance are external steps.

Review the exact signed artifacts, public certificates, immutable source, checksums, approved screenshots with truthful capture provenance and final EN/ES/CA copy before separately authorized merge/publication. First-Android release wording does not waive those gates.
