# Sudokura Play privacy and Data safety — evidence worksheet (NOT final declarations)

Base source examined: `76fdc4811a4a83541e1e83c4432dc0f2785f37d3`.
Re-audit when app source or SDK dependencies change. All responses below are
preparation notes, not Play Console selections or an approved legal policy.

## Source-backed observations

- Application ID: `org.cajapersonal.sudokura` (`android/app/build.gradle`).
- Declared app manifest does not request `android.permission.INTERNET` or
  broad storage permissions; check the **merged** production manifest and all
  embedded SDK/SDL code again before making final privacy declarations.
- Android `allowBackup=false`, explicit backup and data-extraction policy
  owners are present. User profile, saves, settings, and history are treated as
  app-local; independent runtime/provider verification remains necessary.
- The publicly reviewed copy states that there are no accounts, ads or
  analytics. An assertion of "no personal data collected" in Play Console
  requires a complete code/SDK/network inventory and final runtime confirmation,
  not only reading this worksheet.
- The isolated store-preparation source adds an in-app About policy link in
  English, Spanish and Catalan, using the exact allowlisted Site fragment.
  Device/browser behavior still requires focused Android runtime verification.
- The maintainer published the existing Site policy at
  `https://sudokura.cajapersonal.org/#privacy` via ChatGPT Sites on 2026-11-05.
  In a connected browser, the English section was inspected directly under the
  heading `Sudokura Privacy Policy`; it names the app and developer Santiago
  Rodriguez, describes app-private saves/notes/history/settings, uninstall and
  clear-data deletion, Android backup/transfer disabled, no app accounts, ads,
  analytics or cloud sync, and voluntary external links. Its contact mechanism
  is `https://github.com/santirodriguez/Sudokura/issues`.
  The visible EN/ES/CA selector and ChatGPT Sites' report support translation
  availability; ES/CA actual policy text and anonymous access were not separately
  inspected. The `#privacy` fragment is client navigation on `/`, not a distinct
  server route. Play Console acceptance is pending.

## Content to verify/update in the existing public HTML privacy page

1. Policy title "Sudokura Privacy Policy", developer/app identity and current
   privacy contact mechanism (verified and approved by the maintainer).
2. What the app does and does **not** collect, transmit or share; distinguish
   user-initiated external browser links from app data collection.
3. Local saves, notes, puzzle history, settings, location of storage, optional
   media, network behavior and any bundled SDK or generated diagnostic data.
4. User data retention, deletion/uninstall behavior and Android backup and
   device-transfer exclusions; do not claim recovery after uninstall.
5. Children's/audience statements only after a real product/Console decision.
6. Site-specific analytics/cookies and third-party handling covered by the Site's
   own policy rather than incorrectly attributing them to the app.
7. Date, change policy, approved translations EN/ES/CA, and verified contact.

## Before filling Play Console

- [ ] Full merged manifest + source/network/SDK audit on the intended APK/AAB.
- [x] Candidate HTTPS HTML Privacy URL published and English policy content visible in the connected browser: `https://sudokura.cajapersonal.org/#privacy`.
- [ ] Confirm anonymous/no-login access, ES/CA language content and Play Console's acceptance of this fragment URL before final submission.
- [ ] Verify the discoverable multilingual About link on the exact Android
      candidate, including external browser navigation and Back behavior.
- [ ] Current accurate Data safety answers, audience/content rating and ads.
- [ ] Developer contact details, app ownership, account type and any required
      closed-testing obligations verified in the actual Console.
- [ ] Distinct existing Play upload key confirmed **privately**, not uploaded
      or rotated as part of documentation preparation.
- [ ] Separate authorization for Console registration, app signing enrollment,
      actual signed AAB upload, private testing or store submission.

Official policy: https://support.google.com/googleplay/android-developer/answer/10144311
