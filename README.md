# Sudokura

<p align="center">
  <img src="assets/branding/source/sudokura-head.png" alt="Sudokura" width="480">
</p>

<p align="center">
  <strong>A little time for a good puzzle.</strong>
</p>

<p align="center">
  Classic Sudoku, daily challenges, and just enough help when you need it.<br>
  No accounts, no ads, no analytics. Just you and the grid.
</p>

<p align="center">
  <a href="https://sudokura.cajapersonal.org/">Official website</a> ·
  <a href="#download">Download</a> ·
  <a href="#play-your-way">Features</a> ·
  <a href="docs/images/README.md">Screenshots</a> ·
  <a href="docs/RELEASE_NOTES_1.4.0.md">What's new</a>
</p>

---

## Download

<p align="center">
  <a href="https://github.com/santirodriguez/Sudokura/releases/latest"><img src="https://img.shields.io/badge/Windows-x64-789BE8?style=for-the-badge&logo=windows11&logoColor=white" alt="Windows x64 — installer and portable EXE" height="28"></a>
  <a href="https://github.com/santirodriguez/Sudokura/releases/latest"><img src="https://img.shields.io/badge/Linux-AppImage-343F57?style=for-the-badge&logo=linux&logoColor=white" alt="Linux AppImage" height="28"></a>
  <a href="https://github.com/santirodriguez/Sudokura/releases/latest"><img src="https://img.shields.io/badge/macOS-arm64-343F57?style=for-the-badge&logo=apple&logoColor=white" alt="macOS Apple Silicon DMG" height="28"></a>
  <a href="#android"><img src="https://img.shields.io/badge/Android-APK-3D8065?style=for-the-badge&logo=android&logoColor=white" alt="Android APK" height="28"></a>
</p>

<p align="center">
  <sub>Windows installer or portable · Linux AppImage · macOS Apple Silicon · Android ARM64</sub>
</p>

<p align="center">
  <a href="https://apps.obtainium.imranr.dev/redirect?r=obtainium://add/https://github.com/santirodriguez/Sudokura"><img src="https://raw.githubusercontent.com/ImranR98/Obtainium/main/assets/graphics/badge_obtainium.png" alt="Add Sudokura to Obtainium" height="48"></a><br>
  <sub>Follow Android releases with Obtainium. APK downloads start with v1.4.0.</sub>
</p>

<details>
<summary>Installation and download verification</summary>

- **Requirements:** Windows 11 x64 · Linux x86_64 · macOS 15+ Apple Silicon · Android 9+ ARM64.
- **Downloads:** buttons open the latest published release. Android downloads require the v1.4.0 release to be published; F-Droid and Google Play are separate channels.
- **Windows:** choose the installer or the single-file, installation-free portable EXE. Unsigned Windows builds may trigger a SmartScreen reputation warning.
- **Linux:** make the AppImage executable with <code>chmod +x Sudokura-*-Linux-x64.AppImage</code>. If FUSE is unavailable, launch with <code>--appimage-extract-and-run</code>.
- **macOS:** the Apple Silicon DMG is experimental, ad-hoc signed, and not Apple-notarized.
- **Integrity:** the [release page](https://github.com/santirodriguez/Sudokura/releases/latest) includes <code>SHA256SUMS.txt</code> and a matching Build-Info file.

</details>

## A familiar board, carefully made

<p align="center">
  <a href="docs/images/sudokura-v1.4.0.png">
    <img src="docs/images/sudokura-v1.4.0.png" alt="Sudokura desktop gameplay: Sudoku board, notes, number keys and Save controls" width="900">
  </a>
  <br>
  <sub>The familiar desktop board — approved screenshot reused from v1.3.9. New journey screens and Android are not pictured.</sub>
</p>

<table>
<tr>
<td width="33%" valign="top">
<strong>Thoughtful puzzles</strong><br><br>
Easy, Medium, and Hard puzzles with unique solutions and difficulty based on human-solving techniques.
</td>
<td width="33%" valign="top">
<strong>Comfortable controls</strong><br><br>
Play with a keyboard, mouse, or desktop touchscreen. Notes, Undo/Redo, hints, and themes are close at hand.
</td>
<td width="33%" valign="top">
<strong>Yours to keep</strong><br><br>
Sudokura works offline. Your games, settings, and history stay in local storage, not on somebody else's server.
</td>
</tr>
</table>

## Play your way

- **Classic** — enjoy the puzzle at your own pace.
- **Strikes** — every mistake counts.
- **Time Attack** — solve against the clock.
- **Daily Puzzle** — a consistent challenge for each local calendar day.

Save and resume an attempt in **Saved Games**, celebrate wins in **Your journey**, or choose **Try again** to revisit the same puzzle. Use Notes, Hint, Verify, Undo/Redo, and Restart whenever you need them.

Choose light or dark themes, reduced motion, and optional music and sound effects. Available in **English, Español, and Català**.

<details>
<summary>Keyboard shortcuts</summary>

| Action | Shortcut |
|---|---|
| Move / enter / clear | Arrow keys or WASD · 1–9 · Backspace/Delete |
| Notes | N · Shift+1–9 · right-click |
| Undo / Redo | Ctrl+Z / Ctrl+Shift+Z (or Ctrl+Y); Cmd equivalents on macOS |
| Save / Saved Games | Ctrl+S / Ctrl+O; Cmd equivalents on macOS |
| Hint / Verify / Pause | H · Ctrl+Enter · P |
| Theme / Language / Audio | T · L · V |
| Help / About | F1 / F2 |

All essential actions are also available through the interface.

</details>

## Private, even when you take a break

Sudokura has no account, cloud sync, or telemetry. Your games, settings, and history stay on your device.

<details>
<summary>How saves, retries, and upgrades work</summary>

**Save** keeps the latest state of the same attempt. Saved Games holds explicit saves and one current autosave; **Your journey** keeps wins and interrupted or lost attempts. Linked retries preserve earlier results and move into Achieved when you win. Backgrounding only autosaves; leaving a played attempt offers Save, Abandon, or Cancel.

Existing compatible v1.2 and v1.3 desktop data keeps its migration and recovery safeguards. If you're moving between versions, **keep your complete game-data folder**. See [saves, recovery, and rollback](docs/REPORTING_ISSUES.md).

</details>

## Android

**1.4.0 is Sudokura's first Android release.** The same offline Sudoku engine, portrait and landscape play, touch controls, and Your journey — now on your phone.

Requires **Android 9+ on ARM64**. Download the signed APK from [GitHub Releases](https://github.com/santirodriguez/Sudokura/releases/latest), or follow the repository with the Obtainium button above. Android downloads require a published APK release. F-Droid and Google Play availability will be announced separately.

<details>
<summary>Android compatibility, updates, and local progress</summary>

The signed production candidate has been tested on a real phone. It shares the desktop version number, 1.4.0.

Progress stays on the device; automatic cloud backup, device transfer and migration from private prototypes are not supported. Keep an installation that holds your progress: changing to a store with a different signing certificate may prevent an in-place update. Full TalkBack navigation and 16 KB runtime compatibility have not been verified.

See [Android installation and signing](android/RELEASING.md) for channel and update details. Obtainium discovery and updates require a published APK release; adding the repository does not create a release or establish store availability.

</details>

## Build & contribute

Sudokura is written in **C11**, using SDL2, SDL2_ttf, and SDL2_mixer.

<details>
<summary>Build from source</summary>

Install a C compiler, <code>pkg-config</code>, SDL2, SDL2_ttf, SDL2_mixer, Python 3, and Go.

~~~sh
make assets
make
make test
make test-ui
./sudokura
~~~

More build and engineering details are in [Development](docs/DEVELOPMENT.md).

</details>

Found a bug or have an idea? [Open an issue](https://github.com/santirodriguez/Sudokura/issues). For release history, screenshots, development notes, and troubleshooting, visit the [documentation index](docs/README.md).

## Support Sudokura

Sudokura is a free, open-source game made by [Santiago Rodriguez](https://github.com/santirodriguez). If you enjoy playing it, you can support its continued development.

<p align="center">
  <a href="https://cajapersonal.org/donate/"><img src="docs/images/donate.svg" alt="Support Sudokura — Donate" height="46"></a>
</p>

[English](https://cajapersonal.org/donate/) · [Español](https://cajapersonal.org/donaciones/) · [Català](https://cajapersonal.org/donacions/)

<sub>Music: Cozy Puzzle Jingle & Result by MintoDog ([CC0](https://opengameart.org/content/cozy-puzzle-jingle-result)) · Language flags by [lipis/flag-icons](https://github.com/lipis/flag-icons) (MIT)</sub>

**[GPLv3](LICENSE)** · © 2025–2026 [Santiago Rodriguez](https://github.com/santirodriguez)
