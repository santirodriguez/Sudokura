First public Android release, alongside the Sudokura 1.4.0 desktop update.

> Unreleased draft. Publication and store availability remain pending; internally signed APKs are verified; final copy and genuine screenshots require maintainer review.

### Sudoku on Android

The same offline Sudoku engine now has touch-first help, portrait and landscape layouts, a cleaner More panel and shared save/recovery behavior. Earlier Android builds were private debug prototypes; this is planned as the first public Android version, **1.4.0**.

### Your journey

Celebrate wins in Achieved and revisit lost or interrupted attempts with Try again. Replays keep the original puzzle and earlier attempts, with separate time/error statistics and assistance/practice labels. Saved Games holds explicit saves and the current autosave. Merely opening a board is not a failed attempt; phone backgrounding only saves progress.

### More reliable interruptions

Confirmations and recovery run through the main event loop, preserving independent pause reasons and stable saved-game identities. Compact auxiliary screens have readable scrolling controls. The Android semantic bridge has automated coverage; a full physical TalkBack journey is not yet accepted.

### Installation and updates

The production Android app uses `org.cajapersonal.sudokura` and remains separate from test installations. Automatic migration from the prototype is not promised. Do not uninstall an installation that holds progress to work around a signing mismatch. Final signed APKs, update acceptance, F-Droid and Play availability must be verified before replacing this draft with publication copy.
