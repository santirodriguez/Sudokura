# Android production releases: signing and safe updates

Solving the Sudoku is the fun part. This page covers the equally important part: making sure you're installing the right APK and an update doesn't take your saved games with it.

Sudokura 1.4.0 was the **first public Android distribution version** after private debug prototypes, using Android `versionCode 1`. Sudokura 1.4.1 is the next production update, with `versionCode 2` and the same original signing certificate; the maintainer tested an in-place install over 1.4.0 with previous saved games intact. Only [GitHub Releases](https://github.com/santirodriguez/Sudokura/releases) determines what is **actually published**. Internal signing, store submission, main-branch merge, tags and publication have separate approvals.

## One production identity

| Variant | Application ID | Version | Intended use |
| --- | --- | --- | --- |
| Prototype debug | `org.sudokura.prototype.debug` | `1.4.1-prototype` / 2 | Separate test installation; ephemeral debug signer; no automatic migration |
| Original published production | `org.cajapersonal.sudokura` | `1.4.0` / 1 | First public signed Android APK; historical baseline |
| Production update candidate | `org.cajapersonal.sudokura` | `1.4.1` / 2 | Unsigned Gradle APK/AAB; original-key signed direct APK only after authorized manual GitHub Actions run |

The production ID was confirmed on November 3, 2026. The internal Java namespace remains independent of the application ID; retain the JNI class names. The production label is Sudokura. Prototype data is separate; no automatic migration or uninstall workaround is promised. Keep backup/device transfer excluded and preserve local progress.

The visible version comes from the MAJOR/MINOR/PATCH components in `src/version.h`. The independent `SUDOKURA_VERSION_ANDROID_CODE` in the same header starts at 1 and is shared by GitHub/Obtainium, F-Droid and Google Play for equivalent releases. It must stay between 1 and 2100000000. Codes must increase for every subsequently distributed production build, including prereleases; never replace distributed bytes under the same version/code. Before an actual store upload, verify the highest consumed code in that store. An initial F-Droid or Play listing must identify the actual independently accepted version/code in that store; do not assume its first version equals GitHub's original 1.4.0/code 1. Internal production candidates used code 10400; Android refuses a normal downgrade to code 1. Preserve installations holding progress: no uninstall/data-clear workaround or automatic progress migration is provided. Matching version numbers alone do not establish cross-store signing compatibility. F-Droid and Play acceptance must be checked independently; GitHub publication does not establish their availability.

## Build outputs and verified signing

The reusable Android workflow builds only unsigned Android candidates in `android-debug`. The separate `release.yml` workflow can perform manually authorized original-key signing. The available outputs are:

- `sudokura-android-debug-prototype`: the installable debug APK and structural receipts.
- `sudokura-android-unsigned-production`: the unsigned production APK, unsigned Play AAB, source/version/checksum receipts and APK/native/alignment audits.
- `sudokura-android-signed-github`: the signed direct APK, verified app certificate and source-bound manifest/checksum receipts.
- `phase9-release-bundle`: four desktop application packages, shared Build-Info and checksums, produced by ordinary CI.
- `release-bundle`: the five application packages (including the original-signed direct APK), shared Build-Info and checksums, produced only by a separately approved release workflow.

The production APK is not installable until signed. CI validates the AAB with official bundletool 1.18.3 and a pinned checksum, but does not establish Play acceptance. Ordinary CI and the reusable Android builder do not reference signing credentials. Only an explicitly dispatched `release.yml` run on the exact approved `release/1.4.1` HEAD may reach the `android-sign` job, which rechecks the full 40-character source SHA and verifies the unsigned APK/AAB before accessing the existing GitHub Actions repository Secrets. This is a **workflow-level guard, not a restriction on other workflows that privileged repository contributors can modify**: repository-level secrets remain available to authorized workflows in this repository. No Environment setup or secret migration is required for this approach. Tag pushes do not automatically sign, create drafts or publish; tag-based release packaging would require separately approved re-enablement. The Play upload JKS stays separate and is never used by this job. A signed Actions artifact is not a public release. Native 16 KiB alignment is not 16 KiB runtime acceptance.

From an already equipped environment, build the pinned sources/assets and run:

```sh
bash android/gradlew --no-daemon :app:assemblePrototypeDebug :app:assembleProductionRelease :app:bundleProductionRelease -PsudokuraFont=/verified/path/DejaVuSans.ttf -PsudokuraFontLicense=/verified/path/font-copyright
python3 android/scripts/audit_apk.py /path/to/aapt android/app/build/outputs/apk/production/release/app-production-release-unsigned.apk --variant release --require-unsigned
```

Do not install tooling on the maintainer's computer merely to reproduce CI. The signed direct APK name is derived from source version (`Sudokura-<version>-Android-arm64.apk`; the published v1.4.0 asset uses `Sudokura-1.4.0-Android-arm64.apk`). Files containing `unsigned` are staging artifacts, not public downloads. AABs remain internal Play preparation, not GitHub public release assets. Desktop release assets and their existing checks remain unchanged.

## Existing signing identities and recovery

The two independent Sudokura keys have already been created. The app key signed the published v1.4.0 direct APK; future signing requires a separately approved manual `release.yml` run. The Play upload key remains separate. Do not generate replacements or repeat initial setup for a normal build, update or password change. Never reuse another application's keys.

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

The established direct-APK signing JKS is unchanged. The manual `android-sign` job in `.github/workflows/release.yml` uses the **existing** GitHub Actions repository Secrets without moving or duplicating their values:

- `SUDOKURA_APP_KEYSTORE_BASE64`: encoded original app-signing JKS.
- `SUDOKURA_APP_KEYSTORE_PASSWORD`: its existing store/key-entry password.

The alias remains `sudokura`; no new signing key or Play upload key is needed. The original JKS uses the same value for store and key-entry passwords. The manual run must select `release/1.4.1` in GitHub Actions, be initiated by `santirodriguez`, and supply its exact current HEAD SHA as `approved_source_sha`. Source/actor/event/version checks run before signing. Ordinary pushes, PRs, tags, other refs and mismatched SHAs cannot trigger this signing job through the checked-in workflow. **Because these are repository Secrets and the release branch is currently unprotected, this is not independent authorization against someone able to change privileged workflows.** Review the final exact SHA before any later authorized run; no run is approved by this documentation.

Keep private JKS files and password records outside source control, chat, Issues and ordinary build artifacts. Base64 is an encoding, not encryption. Do not echo secret values or enable shell tracing while handling them. Keep JKS files mode 0600 and their private directory mode 0700. Preserve role-labelled `01-app-signing/`, `02-play-upload/` and `public/` groups in private custody; only exported public certificates belong in the last group.

Changing a JKS password does not by itself create a new app-signing certificate. It also does not invalidate a previously copied keystore protected by the old password. For subsequent CI signing, update both the uploaded JKS content and its matching password secret if they changed. Keep any store/key-entry passwords consistent with the workflow, or deliberately adapt the configuration. Do not rotate the app's signing identity as a password-change workaround. Removing CI secrets prevents future CI signing but does not revoke earlier signed APKs or copies of the private key; local signing remains possible with the original key.

### Backup and recovery

The maintainer reports that both JKS files are backed up. An independent restore exercise has not been verified. Keep encrypted/offline recovery copies and separately protected password records; a second copy on the same disk is not independent recovery.

Restore copies into an isolated private directory without overwriting the originals. Open each restored JKS interactively with its recorded store password and alias, and compare the exported public certificate fingerprint with the table above. Listing a certificate validates access to the keystore, not the ability to use a separately password-protected private key: verify the key-entry password through an isolated signing check when exercising full recovery. Record recovery as verified only after the appropriate check succeeds.

If a key file or password is missing, recover the exact original rather than generating a replacement. Never uninstall a progress-bearing app to hide a signing mismatch. Preserve the long-lived app certificate for future direct updates.

For Play, propose enrolling the original app-signing identity and using the separate upload key, so Play does not silently introduce a different installed-app signer. Actual enrollment and private-key transfer need separate authorization. Verify the live Console certificate before relying on enrollment. F-Droid use of the developer signer requires an accepted reproducible-build recipe and signature verification; it is not automatic. A different store signer can block in-place cross-store updates. Do not promise channel switching until tested with retained data.

The direct APK uses the pinned signing toolchain. Before claiming F-Droid developer-signature/reproducible-build support, validate that toolchain against the applicable signature-copy/rebuild process. Preserve the same app certificate for future direct updates. Do not select a different compiler/NDK merely to copy another application's signing configuration.

## Internal combined candidate

The separately authorized manual signing job signs and verifies the APK, then emits `artifact-manifest-android.json`, `SHA256SUMS-android.txt` and `build-provenance-android.txt`. These bind source/version, application ID, app certificate, APK bytes and signed audit receipts. They do not certify device, reproducibility, store or 16 KiB runtime acceptance.

When the manually dispatched release packaging workflow passes its source guard and original-key signing, its combined bundle includes the signed APK alongside four desktop applications, shared `SHA256SUMS.txt` and `Sudokura-<version>-Build-Info.json`. `scripts/release_bundle.py --help` exposes an explicit `--include-android` option on each subcommand; the desktop-only contract remains available when Android is not built. Missing/mismatched Android bytes, source, identity or signer fail validation.

### Exact public upload handoff

For a pre-publication audit or separately authorized manual upload, use an accepted exact-source signed `release-bundle` from the approved release packaging workflow, not the desktop-only `phase9-release-bundle` from ordinary CI. The accepted signed bundle contains all five application packages and their shared Build-Info/checksums. Set `SOURCE_COMMIT` to that specific candidate's complete 40-character Git commit SHA. The export command rechecks every file against the embedded per-platform manifests, exact source commit, version and Android signer, then stages only seven public files in a new directory:

```sh
python3 scripts/release_bundle.py export \
  --artifacts-dir /path/to/extracted/release-bundle \
  --output-dir /path/to/new/public-upload \
  --version 1.4.1 --source-commit "$SOURCE_COMMIT" --include-android
```

This stages files only: it creates no tag, draft, release or network request, and refuses an existing output directory. Do not substitute unsigned APKs, AABs, per-platform receipts or another run's checksums. Keep the downloaded Actions ZIP digest and source commit with the acceptance record.

For publication, separately authorize the **final reviewed release source**, pre-publication public documentation/screenshots, merge into `main`, tag and the seven public assets. At this stage `release.yml` runs **only on explicit `workflow_dispatch`** from the approved `release/1.4.1` SHA and generates a source-verified staging bundle; it does not respond to tag pushes, create a draft or publish. Choose and separately approve a secure publication mechanism *before* the definitive release. One possible route is a separately reviewed workflow change for an exact-source draft and uploaded-asset validation; do not assume tag-based signing is currently enabled. For a draft, verify every uploaded file against the same final source and checksums using `scripts/release_bundle.py validate-assets --version 1.4.1 --assets-json draft-release-assets.json --include-android`, followed by its published mode after release. A successful manual build is not publication authorization, and signed artifacts from an older source SHA are not automatically valid for a later documentation or workflow commit.

The current manual release workflow runs unsigned Android compilation, Windows/Linux/macOS packaging and a separate original-key signer from the **same approved source**; it assembles five applications and shared checksums only after all required jobs pass. There is no automatic tag-signing or draft-creation route in this candidate. The manual signing job must fail closed on missing Secrets or mismatched source, and source SHA cannot be an arbitrary historical ref. Normal branch CI creates no tag or draft. Any future tag/draft automation requires its own design review and explicit approval.

## Distribution acceptance and limitations

The November 4 acceptance amendment in the [blueprint](../docs/V1.4.0_BLUEPRINT.md) and [roadmap](../docs/ANDROID_RELEASE_PLAN.md) defers full manual accessibility, extended performance measurements and further emulator debugging. Preserve their unverified status and public limitations; known critical defects, source/signing/checksum integrity, and screenshot/public-copy review remain release requirements. Independently rebuild the same exact unsigned source and compare bytes before claiming reproducibility. The maintainer confirmed one in-place public 1.4.0 to originally signed 1.4.1 candidate update retaining saved games without uninstalling. Preserve this exact-source acceptance evidence, and record any separately authorized follow-up clean-install or upgrade checks without deleting user data. Store submissions and acceptance are external steps.

Review the exact signed artifacts, public certificates, immutable source, checksums, approved screenshots with truthful capture provenance and final EN/ES/CA copy before separately authorized merge/publication. First-Android release wording does not waive those gates.
