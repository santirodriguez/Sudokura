Your Sudoku progress should still be there when you return. This update makes switching and recovering saved games more dependable, while keeping Sudokura's familiar board and controls.

### Saved games you can trust

- Loading another game after a finished, lost or interrupted attempt no longer triggers an unnecessary Save-and-leave request for that completed attempt.
- A started game that has not been saved still gives you a choice before you leave it.
- Interrupted results retain the correct attempt identity in your history, helping Saved Games, Achieved and Try again stay consistent.

### A safer Android update

Sudokura 1.4.1 keeps the same production Android application ID (`org.cajapersonal.sudokura`) and original app-signing certificate as the public 1.4.0 APK. The maintainer installed 1.4.1 directly over 1.4.0 **without uninstalling**, and confirmed that existing saved games were retained.

**Update the installed app rather than uninstalling it.** Progress is stored locally, with no cloud sync, automatic Android backup, device transfer or automatic migration from private debug prototypes. Switching to an APK signed by a different store may prevent a direct update.

The 1.4.1 Android package is versionCode 2 and supports Android 9+ on ARM64. Structural native-library and APK alignment checks passed; **live 16 KiB page-size execution and complete TalkBack navigation remain unverified**.

### Your familiar Sudokura

You still get Classic, Strikes, Time Attack and dated Daily puzzles; three difficulties; pencil notes, Undo/Redo, Hint and Verify; Your journey and linked retries; light/dark themes; optional audio; and English, Español and Català. The new release does not redesign the interface or alter puzzle generation.

The approved desktop and phone screenshot gallery shows authentic 1.4.0 captures of the shared interface that remains representative of 1.4.1. They are not new 1.4.1 screenshots.

### Downloads and compatibility

The supported package lineup remains Windows x64 (installer and portable EXE), Linux x64 (AppImage), experimental macOS 15+ Apple Silicon (DMG), and a signed ARM64 Android APK. GitHub Releases is the authoritative source for published files and their `SHA256SUMS.txt` and Build-Info. Windows packages are unsigned; the macOS build is ad-hoc signed but not Apple-notarized.

Google Play and F-Droid are separate distribution channels. Their listings and cross-store update compatibility are not implied by this GitHub release.
