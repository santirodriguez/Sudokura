# Sudokura v1.0

A compact, modern **Sudoku** written in **C + SDL2**, with responsive UI, notes, strict mode, hints, verification, multiple play modes (Classic / Strikes / Time Attack), and dark/light themes.

[![screenshot](https://github.com/santirodriguez/Sudokura/blob/main/screenshot.png)](https://github.com/santirodriguez/Sudokura/blob/main/screenshot.png)

---

## Table of Contents
- [Features](#features)
- [Controls](#controls)
- [Download and Releases](#download-and-releases)
- [Build from Source](#build-from-source)
  - [Prerequisites](#prerequisites)
  - [Linux (Fedora / Ubuntu / Debian / Arch)](#linux-fedora--ubuntu--debian--arch)
  - [macOS (Homebrew)](#macos-homebrew)
  - [Windows (MSVC + vcpkg)](#windows-msvc--vcpkg)
  - [Windows (MinGW-w64, native on Windows)](#windows-mingw-w64-native-on-windows)
- [Command-line Options](#command-line-options)
- [Packaging Releases](#packaging-releases)
  - [Linux AppImage](#linux-portable-recommended-appimage)
  - [Windows ZIP](#windows-zip)
  - [macOS ZIP](#macos-zip)
- [Development Notes](#development-notes)
- [License](#license)

---

## Features

- Three modes:
  - **Classic** – play at your pace
  - **Strikes** – 3 wrong moves = lose
  - **Time Attack** – solve under 10:00
- Notes (pencil marks): toggle with **N** or hold **Shift** while entering numbers; also click sub-cells in the mini 3x3 grid inside a cell
- Hint: fills the current cell correctly
- Verify: checks row, column, and box conflicts (does not reveal the solution)
- Strict mode: blocks illegal placements (toggle with **M**)
- Dark / Light theme (toggle **T**)
- Responsive UI: side panel right or stacked, works on Wayland and X11
- Robust font discovery: runs out-of-the-box on Linux, macOS, and Windows

---

## Controls

- **Mouse:** click to select a cell. In Notes mode (or right-click), click a sub-cell to toggle a pencil mark  
- **Keyboard:**
  - Move: Arrows / WASD
  - Place number: 1..9 (main row or numpad)
  - Clear: 0 / Backspace / Delete
  - Notes mode: N (or hold Shift while typing 1..9)
  - Strict/Free: M
  - Hint: H
  - Theme: T
  - Pause: P
  - Menu: ESC

---

## Download and Releases

Ready-to-play builds for **Linux**, **Windows**, and **macOS** are available in the  
[Releases section](https://github.com/santirodriguez/Sudokura/releases).

Each release includes:
- AppImage for Linux (portable, no install required)
- ZIP for Windows (contains .exe and required DLLs)
- ZIP for macOS (standalone binary)

You can test or play directly by downloading the package for your platform and running it.  
No compilation required.

---

## Build from Source

The game is a single C file: `sudokura_sdl.c`

### Prerequisites

- A C compiler (gcc, clang, or MSVC)
- SDL2 and SDL2_ttf development packages
- pkg-config (for Linux and macOS builds)

### Linux (Fedora / Ubuntu / Debian / Arch)

```bash
# Fedora
sudo dnf install -y gcc SDL2-devel SDL2_ttf-devel pkgconf-pkg-config

# Ubuntu/Debian
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

### Windows (MSVC + vcpkg)

```bat
:: Install
vcpkg install sdl2 sdl2-ttf

:: Build in "Developer Command Prompt for VS"
cl /std:c11 /O2 /W3 sudokura_sdl.c ^
  /I"%VCPKG_ROOT%\installed\x64-windows\include" ^
  /link /LIBPATH:"%VCPKG_ROOT%\installed\x64-windows\lib" SDL2.lib SDL2_ttf.lib

:: Rename for releases
ren sudokura.exe Sudokura-v1.exe
```

### Windows (MinGW-w64, native on Windows)

```bash
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf
x86_64-w64-mingw32-gcc -std=c11 -O2 -Wall -Wextra sudokura_sdl.c -o Sudokura-v1.exe \
  $(x86_64-w64-mingw32-pkg-config --cflags --libs sdl2 SDL2_ttf) -lm
```

---

## Command-line Options

- `--font /path/to/font.ttf` – force a font (normally not needed; auto-discovery picks an installed TTF or OTF)

---

## Packaging Releases

### Linux portable (recommended): AppImage

1. Build the Linux binary: `./Sudokura-v1`
2. Create a minimal desktop file (for example `sudokura.desktop`) and icon, then:

```bash
wget -O linuxdeploy.AppImage https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod +x linuxdeploy.AppImage

mkdir -p AppDir/usr/bin
cp Sudokura-v1 AppDir/usr/bin/
cp sudokura.desktop AppDir/
cp icons/sudokura.png AppDir/

./linuxdeploy.AppImage --appdir AppDir --executable AppDir/usr/bin/Sudokura-v1 \
  --desktop-file AppDir/sudokura.desktop --icon-file AppDir/sudokura.png \
  --output appimage

# Resulting artifact:
# Sudokura-v1-x86_64.AppImage
```

This produces a portable file that runs on most modern distros (Ubuntu, Fedora, etc.) without extra dependencies.

### Windows ZIP

If you built with MinGW or MSVC, bundle the produced `Sudokura-v1.exe` with required DLLs if dynamic:
- SDL2.dll, SDL2_ttf.dll, and dependencies (freetype, zlib, brotli, harfbuzz – varies per build)
  
Use `dumpbin /dependents Sudokura-v1.exe` (MSVC) or  
`x86_64-w64-mingw32-objdump -p Sudokura-v1.exe | grep 'DLL Name'` (cross toolchain) to list required DLLs.  

Zip them together as: `Sudokura-v1-windows.zip`

### macOS ZIP

On macOS, compile as above and zip the binary (or wrap into an `.app` bundle if you prefer).  
Name it `Sudokura-v1-macos.zip`.

---

## Development Notes

- The UI uses a shared layout function for both painting and hit-testing, ensuring accurate clicks on Wayland and X11 at any window size
- Font loader searches common system locations on Linux, macOS, and Windows, and falls back gracefully
- The Sudoku generator ensures unique solutions and targets a medium clue count by default

---

## License

**GPLv3** — see <https://www.gnu.org/licenses/>

Copyright © 2025  
[santirodriguez](https://santiagorodriguez.com)
