# Android store preparation: Google Play + F-Droid

Status: **PREPARATION ONLY** (2026-11-05). This document is a durable work checklist, not proof of store acceptance and not an authorization to merge, publish, upload, enroll keys or modify account access.

## Delivery strategy

1. Make the game ready for both stores before the first **public** distribution, but keep GitHub Releases + Obtainium as the intended first public channel.
2. Keep the accepted `release/1.4.0` / PR #17 free of unapproved runtime changes. The maintainer uploaded and approved two store graphics that were byte-preservingly named and staged in Fastlane on release HEAD `4b7a7a1e9c70386195de7c709ded3e858a3ab356`. The preparatory branch remains independent and must reconcile that artwork before integrating any future code. Do not merge the PR or create a release tag.
3. Reconcile the completed store work before the GitHub release decision. If privacy/API code changes are included in 1.4.0, promote and validate them **before** tagging. Otherwise carry them into the next release, with a new Android versionCode (e.g. 1.4.1/code 2), without replacing already published 1.4.0/code 1 bytes.
4. A private Google Play test track is not a public launch, but account access, enrollment, private-key transfer, uploads and testing still require separately scoped authorization.
5. Never promise seamless cross-store updates unless package ID and installed-app signing certificate are identical and the update with existing user data has actually passed.

## Confirmed upstream inventory (recheck at each release)

- Android ID: `org.cajapersonal.sudokura`; v1.4.0/versionCode 1; minSdk 28, target/compile SDK 36; production ARM64.
- Canonical English six 1080x1920 24-bit RGB screenshots and titles/short/full descriptions in en-US/es/ca already live in `fastlane/metadata/android/`. The screenshots are English, **not** localized Spanish/Catalan images.
- `android/app/src/main/AndroidManifest.xml` has backup disabled and no declared INTERNET permission. No ads or analytics are claimed in approved public copy. Re-audit app/SDK and merged manifest before completing the Play Data safety form.
- CI has successfully produced a developer-signed direct APK and a structurally validated **unsigned** production AAB. That AAB is not a signed Play-upload artifact.
- The app-signing keystore and the **distinct Play upload key** already exist. Keep both original keys, certificate fingerprints, and protected backups; no replacements, copies in chat/repository, or automated enrollment.
- Original in-app icon `assets/branding/source/sudokura-512.png` remains an indexed-color PNG for launcher generation. A distinct **user-uploaded** 512x512 32-bit RGBA Play icon is now available at `assets/branding/source/sudokura-store-icon-512.png` and Fastlane `en-US/images/icon.png` (same Git blob `839d44989b0f808ff61057078983cca175fb5670`), 325336 bytes. Do not overwrite or redesign either master.
- User-uploaded 1024x500 24-bit RGB feature graphic is now in `assets/branding/source/sudokura-store-feature-graphic-1024x500.png` and `fastlane/metadata/android/en-US/images/featureGraphic.png` (same Git blob `dc6779671abaf93ffd02e4961262acaccd4a8e03`, 751271 bytes). Google Play required size/encoding passed, with a final store-preview crop/mask check recommended. **Published privacy section:** `https://sudokura.cajapersonal.org/#privacy`; the maintainer reported a successful ChatGPT Sites deployment with three languages, and the English policy content was actually inspected in a connected browser on 2026-11-05. In-app Privacy entry, Play Console Data safety, the F-Droid build recipe, offline build and reproducibility proof remain outstanding.
- Static 16 KiB ELF/ZIP checks passed on the signed APK; live 16 KiB ARM64 behavior and full TalkBack journey remain unverified. These were explicitly deferred for the GitHub release path, not passed.

## Track 1: Privacy + Play policies (reusable even without Play publication)

- [ ] Independently audit the final Android manifest, network/dependency paths, local profile/save locations, backup/extraction rules, SDK behavior and all external links. Record factual Data safety answers; never infer no data collection from a single permission alone.
- [x] The existing ChatGPT Site now shows `Sudokura Privacy Policy` for the app, naming Sudokura and developer Santiago Rodriguez; it describes local saves/notes/history/settings, uninstall/clear-data deletion, excluded Android backup/transfer, no accounts/ads/analytics/cloud sync, and voluntary external links. The named privacy-inquiry mechanism links to https://github.com/santirodriguez/Sudokura/issues. English page text inspected in the connected browser on 2026-11-05; ES/CA translation availability reported by ChatGPT Sites, not independently read.
- [x] ChatGPT Sites reported deployment of the existing multilingual HTML Privacy section at `https://sudokura.cajapersonal.org/#privacy`; this exact URL was directly displayed with actual English policy content in a connected browser. The language selector has EN, ES, CA controls; Sites reports all three translations work. The fragment `#privacy` is a browser-side anchor, not a separate server route: confirm the final URL also works in an anonymous/incognito browser and is accepted by Play Console when the account is available. Do not assert independent unauthenticated access or Console approval until verified.
- [x] Implement a visible `Privacy policy` / `Política de privacidad` / `Política de privadesa` action in About with the exact policy URL allowlist. SDL keyboard, touch/pointer, Back and accessibility paths are updated; runtime acceptance remains pending.
- [ ] Verify new URL, localization, geometry, SDL navigation and accessibility regression tests in exact-source CI, plus a focused Android runtime check. For any app-code change, verify a fresh signed APK/AAB as applicable. The six approved gameplay screenshots may be reused only after confirming their pictured UI did not change.

## Track 2: Google Play asset and console readiness

- [x] Validate and stage the user's **separate** 512x512 RGBA icon (325336 bytes), preserving the original indexed launcher master. It is now in canonical en-US Fastlane. Preview the rounded Play mask once in Console; avoid clipping key details.
- [x] Validate and stage the user's 1024x500 RGB feature graphic (751271 bytes) in canonical en-US Fastlane. Its artwork is illustrative, not a real in-game screenshot; review crop/safe zones in Play Console before any upload.
- [x] Existing six screenshot PNGs are authentic, visually inspected, 24-bit RGB, and 1080x1920.
- [x] Existing en-US/es/ca title, short_description and full_description copy were jointly approved. Existing screenshot locale remains en-US.
- [ ] Verify screenshot alt-text and all listing fields in Play Console; confirm category, audience/content rating, support/contact details, advertisements, Data safety, distribution territory, accessibility and applicable app-content declarations. No console answers are presumed complete by this document.
- [ ] Determine whether the developer account is already available and its applicable access rules. Personal accounts created after 2023-11-13 ordinarily require a continuous 14-day closed test with at least 12 opted-in testers before production access. Start any actual Console track only after approval; GitHub remains the first intended **public** distribution.
- [ ] With separate approval, sign the source-bound production AAB using the **existing upload key**. Configure Play App Signing to preserve the existing installed-app signing identity where supported and expressly authorized (possibly a protected encrypted PEPK transfer); inspect uploaded certificate fingerprints. Do not upload or enroll keys merely as a preparation convenience.
- [ ] Validate target API 36 and actual support in a 16 KiB environment for native libraries. Structural alignment is necessary but not a full runtime assertion; recheck current Play Console warnings.
- [ ] Only later, after account/content requirements and an independently authorized submission, upload to a suitable private test track and review the generated delivery APK/update path.

## Track 3: F-Droid build and metadata readiness

- [x] Public source repository, GPLv3 license, genuine Fastlane copy/screenshots, and pinned SDL2, SDL2_ttf, SDL2_mixer and Gradle archive digests exist.
- [ ] Verify licenses of every native library, font, icon and audio asset; check that every Gradle/Android build dependency is source-built or accepted by F-Droid policy, with no tracking/non-FOSS dependency. Document any constraints instead of hand-waving them.
- [ ] Make `android/scripts/prepare_sources.py` support a verified **already staged source** path without network during F-Droid's isolated build. Preserve exact archive SHA-256 verification, archive extraction safety, versions and production behavior. Do not introduce a second source vendor or an unverified network fallback.
- [ ] Prepare a candidate `metadata/org.cajapersonal.sudokura.yml` under a local/branch-only review location, **not** in the upstream `fdroiddata` repository yet. Include `RepoType`, `Repo`, license/category/source/issue metadata, and only a build block supported by an actual build. Each build must use an immutable published full commit hash (not an untagged moving branch) and the exact versionName/versionCode.
- [ ] Prepare the F-Droid SDK/NDK, verified SDL sources, canonical DejaVu font/license and generated art path in the appropriate `prebuild`/`build` phase; run `fdroid readmeta`, `fdroid lint`, and a clean isolated, network-controlled source build. The existing `productionRelease` flavor and `app-production-release-unsigned.apk` path are starting evidence, not proof the recipe works.
- [ ] Compare a fresh upstream unsigned/signed APK with the independent F-Droid rebuild using byte-level diffs/`diffoscope`. C/NDK builds are sensitive to paths/toolchains/timestamps. Decide signing only from evidence.
- [ ] Prefer verified upstream developer-signature reproducibility to maintain cross-channel updates. If it cannot be achieved, F-Droid may sign with its own key; document that this prevents in-place channel switching unless a verified migration exists. Reproducibility is encouraged, **not mandatory** for F-Droid inclusion.
- [ ] Once a public tag and validated recipe exist, propose the corresponding merge request to `fdroiddata` **only with separate approval**. Never assert acceptance by F-Droid before its maintainers actually approve and build the app.

## Parallel execution order (minimize critical path)

1. **Immediately, offline/read-only:** audit source, licenses, SDK/NDK, dependency archives, app data practices and store formats. Reuse current Fastlane PNGs and copy rather than generating new screenshots.
2. **Next, low-risk prep:** use the now-known Site Privacy URL `https://sudokura.cajapersonal.org/#privacy` as the candidate in-app/Play link; verify anonymous access and ES/CA text later, and keep the factual Data safety worksheet and F-Droid metadata draft current. Icon, feature graphic and six screenshots already reside in canonical Fastlane on the newer release branch (reconcile that source before merging app changes).
3. **Bounded implementation, requires a separate agreed code-change scope:** add the accessible in-app Privacy page/link; make F-Droid source staging network-independent; add focused tests. Keep this isolated from release/1.4.0 until the same-version versus next-version decision.
4. **Validation:** run source-bound Android CI for any code changes; test Play AAB after authorized signing and actual runtime policy gates; test F-Droid `fdroid build` in its supported environment and, if feasible, reproducibility. Avoid rerunning expensive screenshot capture unless gameplay-visible content changed.
5. **Final decision before GitHub launch:** either integrate the proven fixes into v1.4.0 before merging/tagging, **or** explicitly defer store-only app changes to v1.4.1/code2 and keep GitHub/Obtainium as the first public release. Record remaining Play/F-Droid tasks in source-controlled prep evidence, not in the published release's claims.
6. **After GitHub publication:** check exact tag/source/signed assets and Obtainium discovery; later submit to Play and F-Droid via independent, explicitly approved processes. Preserve the signing/update safety policy for users with saves.

## Gates and owners

| Gate | Automated/assistant preparation | Explicit maintainer or external gate |
|---|---|---|
| Privacy | source/data audit, policy draft, in-app UI code/tests | approve legal/contact copy; authorize Site publication and code integration |
| Store imagery | reuse six screenshot PNGs, inspect/export icon, feature graphic design proposal | approve new visual assets |
| Play | prepare assets/checklist/AAB tests | account identity, Console declarations, private signing key handling, app enrollment, private tests and uploads |
| F-Droid | dependency/license inventory, build recipe and reproducibility proof | third-party GitLab merge request and F-Droid maintainer acceptance |
| GitHub | final PR/source/build review | PR merge, immutable tag, draft acceptance and public release as separately authorized actions |

## Official reference points (reverify at submission)

- Google user-data/privacy rules: https://support.google.com/googleplay/android-developer/answer/10144311
- Google Play graphic and screenshot requirements: https://support.google.com/googleplay/android-developer/answer/9866151
- Play API-level rules: https://developer.android.com/google/play/requirements/target-sdk
- 16 KiB runtime and Play requirement: https://developer.android.com/guide/practices/page-sizes
- Play signing: https://developer.android.com/studio/publish/app-signing
- New personal-account testing: https://support.google.com/googleplay/android-developer/answer/14151465
- F-Droid inclusion: https://f-droid.org/docs/Inclusion_Policy/
- F-Droid packaging: https://f-droid.org/docs/Submitting_to_F-Droid_Quick_Start_Guide/
- F-Droid build recipe reference: https://f-droid.org/docs/Build_Metadata_Reference/
- F-Droid signature reproducibility: https://f-droid.org/docs/Reproducible_Builds/

Do not update this checklist's checkboxes to DONE on inference or intent alone. Record the exact source commit, test output and external acceptance when it occurs.
