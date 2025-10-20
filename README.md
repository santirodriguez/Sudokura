# Sudokura v1.0

A compact, modern **Sudoku** written in **C + SDL2**, with responsive UI, notes, strict mode, hints, verification, multiple play modes (Classic / Strikes / Time Attack), and dark/light themes.

[![screenshot](https://github.com/santirodriguez/Sudokura/blob/main/screenshot.png)](https://github.com/santirodriguez/Sudokura/blob/main/screenshot.png)

---

## Table of Contents
- [Features](#features)
- [Controls](#controls)
- [Download](#download)
- [Build from Source](#build-from-source)
  - [Prerequisites](#prerequisites)
  - [Linux (Fedora / Ubuntu / Debian / Arch)](#linux-fedora--ubuntu--debian--arch)
  - [macOS (Homebrew)](#macos-homebrew)
  - [Windows (MSVC + vcpkg, optional)](#windows-msvc--vcpkg-optional)
  - [Windows (MSYS2 / MinGW-w64)](#windows-msys2--mingw-w64)
- [Command-line Options](#command-line-options)
- [Packaging Releases](#packaging-releases)
  - [Linux AppImage](#linux-appimage)
  - [Windows ZIP](#windows-zip)
  - [macOS ZIP](#macos-zip)
- [Development Notes](#development-notes)
- [Known Notes](#known-notes)
- [License](#license)

---

## Features

- Three modes:
  - **Classic** – play at your pace
  - **Strikes** – 3 wrong moves = lose
  - **Time Attack** – solve under 10:00
- Notes (pencil marks): toggle with **N** or hold **Shift** while entering numbers; also click sub-cells in the mini 3×3 grid inside a cell
- Hint: fills the current cell correctly
- Verify: checks row, column, and box conflicts (does not reveal the solution)
- **Strict mode**: blocks illegal placements (toggle with **M**). Free mode allows them (they still count as mistakes)
- Dark / Light theme (toggle **T**)
- Responsive UI: sidebar on the right or stacked depending on window size
- Robust font discovery: runs out-of-the-box on Linux, macOS, and Windows

---

## Controls

- **Mouse:** click to select a cell. In Notes mode (or with right-click), click a sub-cell to toggle a pencil mark.  
- **Keyboard:**
  - Move: Arrows / WASD
  - Place number: 1..9 (top row or numpad)
  - Clear: 0 / Backspace / Delete
  - Notes mode: **N** (or hold **Shift** while typing 1..9)
  - Strict/Free: **M**
  - Hint: **H**
  - Theme: **T**
  - Pause: **P**
  - Menu / Back: **ESC**

---

## Download

Grab the latest builds for **Linux**, **Windows**, and **macOS** from:  
**➡️ [Releases ›](https://github.com/santirodriguez/Sudokura/releases/latest)**

Each release typically includes:
- **Linux:** AppImage (`Sudokura-v1-x86_64.AppImage`) — portable, no install
- **Windows:** ZIP (`Sudokura-v1-windows.zip`) — includes `Sudokura-v1.exe`, required DLLs, and a fallback font
- **macOS:** ZIP (`Sudokura-v1-macos.zip`) — standalone binary

> Checksums and per-version notes are listed in the corresponding Release page.

---

## Build from Source

Single C file: `sudokura_sdl.c`

### Prerequisites

- A C compiler (**gcc**, **clang**, or **MSVC**)
- **SDL2** and **SDL2_ttf** development packages
- **pkg-config** (Linux/macOS)

### Linux (Fedora / Ubuntu / Debian / Arch)
```bash
# Fedora
sudo dnf install -y gcc SDL2-devel SDL2_ttf-devel pkgconf-pkg-config
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y build-essential libsdl2-dev libsdl2-ttf-dev pkg-config
# Arch
sudo pacman -S --needed gcc sdl2 sdl2_ttf pkgconf
# Build
gcc -std=c11 -O2 -Wall -Wextra \
  sudokura_sdl.c -o Sudokura-v1 \
  $(pkg-config --cflags --libs sdl2 SDL2_ttf) \
  -lm
# Run
./Sudokura-v1
```

### macOS (Homebrew)
```bash
brew install sdl2 sdl2_ttf pkg-config
clang -std=c11 -O2 -Wall -Wextra sudokura_sdl.c -o Sudokura-v1 \
  $(pkg-config --cflags --libs sdl2 SDL2_ttf) -lm
./Sudokura-v1
```

### Windows (MSVC + vcpkg, optional)
```bat
:: Install deps (from a Developer Command Prompt)
vcpkg install sdl2 sdl2-ttf
:: Build (adjust VCPKG_ROOT if needed)
cl /O2 /W3 sudokura_sdl.c ^
  /I"%VCPKG_ROOT%\installed\x64-windows\include" ^
  /link /LIBPATH:"%VCPKG_ROOT%\installed\x64-windows\lib" SDL2.lib SDL2_ttf.lib
:: (Optional) Rename for releases
ren sudokura.exe Sudokura-v1.exe
```

### Windows (MSYS2 / MinGW-w64)
```bash
# In MSYS2's MINGW64 shell:
pacman -S --needed \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-pkg-config \
  mingw-w64-x86_64-SDL2 \
  mingw-w64-x86_64-SDL2_ttf
gcc -std=c11 -O2 -Wall -Wextra sudokura_sdl.c -o Sudokura-v1.exe \
  $(pkg-config --cflags --libs sdl2 SDL2_ttf) -lm
```

---

## Command-line Options

- `--font /path/to/font.ttf` – force a specific TrueType/OpenType font (normally unnecessary; auto-discovery tries local folder, executable folder, and system font paths).

---

## Packaging Releases

If you want to reproduce the release artifacts locally:

### Linux AppImage
```bash
# Assuming you already built ./Sudokura-v1
wget -O linuxdeploy.AppImage \
  https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget -O appimagetool.AppImage \
  https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
chmod +x linuxdeploy.AppImage appimagetool.AppImage
mkdir -p AppDir/usr/bin
cp Sudokura-v1 AppDir/usr/bin/
cat > AppDir/sudokura.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=Sudokura v1
Exec=Sudokura-v1
Icon=sudokura
Categories=Game;
EOF
convert -size 256x256 xc:'#3B4A7A' AppDir/sudokura.png
./linuxdeploy.AppImage --appdir AppDir \
  --executable AppDir/usr/bin/Sudokura-v1 \
  --desktop-file AppDir/sudokura.desktop \
  --icon-file AppDir/sudokura.png
ARCH=x86_64 ./appimagetool.AppImage AppDir Sudokura-v1-x86_64.AppImage
```

### Windows ZIP

Bundle the executable plus its runtime DLLs (SDL2, SDL2_ttf, and any dependencies found by `ntldd` or `dumpbin`). Include a fallback font (e.g., **DejaVuSans.ttf**) in the same folder.

### macOS ZIP

Zip the binary as `Sudokura-v1-macos.zip`. (Optionally package an `.app` bundle and/or sign & notarize for a smoother first run.)

---

## Development Notes

- Shared layout & hit-testing: one geometry function for drawing and mouse interactions → accurate clicks at any window size.
- Font loader: tries current working directory and executable directory first (e.g., nearby `DejaVuSans.ttf`), then common system font paths, then deep scan if needed.
- Sudoku generation: creates a solved board, removes clues down to a **medium** range, and enforces **unique solution**.
- UI polish: layered selection highlights, “same number” highlights, animated selection glow, conflict tinting, and toast messages.

---

## Known Notes

- **macOS:** the uploaded binary is **not tested** by the author and is **not code-signed** or notarized. If Gatekeeper blocks it:
```bash
xattr -dr com.apple.quarantine ./Sudokura-v1
```
- **Windows (portable):** a fallback font (`DejaVuSans.ttf`) is bundled. The app also auto-discovers installed fonts. If you remove the fallback and the system lacks fonts, run with `--font path\to\some.ttf`.

---

## License

**GPLv3** — see <https://www.gnu.org/licenses/>

© 2025 — [santirodriguez](https://santiagorodriguez.com)
