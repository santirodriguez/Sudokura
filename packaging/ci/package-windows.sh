#!/usr/bin/env bash
set -euo pipefail
: "${VERSION:?}"
make assets; make WERROR=-Werror test
windres packaging/windows/sudokura.rc -O coff -o icon.o
gcc -I. -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror sudokura_sdl.c game.c geometry.c i18n.c assets/generated/window_icon.c assets/generated/wordmark.c icon.o -o sudokura.exe $(pkg-config --cflags --libs sdl2 SDL2_ttf) -lm -mwindows
rm -rf dist; mkdir dist; cp sudokura.exe dist/
for pass in 1 2 3 4 5 6; do before=$(find dist -type f | wc -l); ntldd -R dist/*.exe dist/*.dll 2>/dev/null | awk '/=> \/mingw64/{print $3}' | sort -u | xargs -r -I{} cp -n {} dist/; after=$(find dist -type f | wc -l); [[ $before == "$after" ]] && break; done
report=$(ntldd -R dist/*.exe dist/*.dll); printf '%s\n' "$report" | tee dependencies-windows.txt; ! grep -q 'not found' <<<"$report"
font=$(pacman -Ql mingw-w64-x86_64-dejavu-fonts | awk '/DejaVuSans.ttf$/{print $2;exit}'); test -f "$font"; cp "$font" dist/; cp packaging/licenses/DejaVu-FONT-LICENSE.txt dist/
objdump -x dist/sudokura.exe | grep -q '\.rsrc'; SDL_VIDEODRIVER=dummy timeout 30s dist/sudokura.exe --smoke-test
find dist -type f -printf '%f\n' | sort | tee inventory-windows.txt
(cd dist && zip -9 ../Sudokura-v${VERSION}-windows-x86_64.zip *); unzip -t "Sudokura-v${VERSION}-windows-x86_64.zip"; sha256sum "Sudokura-v${VERSION}-windows-x86_64.zip" > SHA256SUMS-windows.txt; du -h Sudokura-*.zip
