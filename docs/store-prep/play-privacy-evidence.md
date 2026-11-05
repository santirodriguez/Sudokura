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
- In-app About currently presents project/support/website links, but no
  identifiable Privacy page/link or policy text was found in current SDL sources.
  A new localized and accessible entry requires a bounded app-code change.
- A verified live public privacy page and its developer/privacy contact do not
  currently have an accepted URL or content. Do not publish or claim an
  unverified `sudokura.cajapersonal.org/privacy/` address.

## Content to draft and verify for a future public HTML privacy page

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
- [ ] Real public HTML policy URL (not PDF, no login or geo restriction).
- [ ] The same policy is accessible from the Android app via a discoverable
      localized link or in-app text.
- [ ] Current accurate Data safety answers, audience/content rating and ads.
- [ ] Developer contact details, app ownership, account type and any required
      closed-testing obligations verified in the actual Console.
- [ ] Distinct existing Play upload key confirmed **privately**, not uploaded
      or rotated as part of documentation preparation.
- [ ] Separate authorization for Console registration, app signing enrollment,
      actual signed AAB upload, private testing or store submission.

Official policy: https://support.google.com/googleplay/android-developer/answer/10144311
