#!/usr/bin/env bash
set -euo pipefail
: "${VERSION:?}"
font=$(pacman -Ql mingw-w64-x86_64-dejavu-fonts | awk '!found && /\/DejaVuSans.ttf$/{value=$2;found=1} END{print value}'); test -n "$font"; test -f "$font"; ./scripts/validate_font.py "$font"; export SUDOKURA_TEST_FONT="$font"
make assets
make WERROR=-Werror test test-ui
windres packaging/windows/sudokura.rc -O coff -o icon.o
gcc -I. -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror sudokura_sdl.c game.c geometry.c i18n.c assets/generated/window_icon.c assets/generated/wordmark.c icon.o -o sudokura.exe $(pkg-config --cflags --libs sdl2 SDL2_ttf) -lm -mwindows
rm -rf dist; mkdir dist; cp sudokura.exe dist/
for pass in 1 2 3 4 5 6; do
  mapfile -t files < <(find dist -maxdepth 1 -type f \( -iname '*.exe' -o -iname '*.dll' \) -print | sort)
  report=$(ntldd -R "${files[@]}"); printf '%s\n' "$report" > dependencies-windows.txt
  mapfile -t deps < <(awk 'tolower($0) ~ /=> \/mingw64\// {print $3}' dependencies-windows.txt | sort -u)
  before=${#files[@]}; for dep in "${deps[@]}"; do test -f "$dep"; cp -n "$dep" dist/; done
  mapfile -t files < <(find dist -maxdepth 1 -type f \( -iname '*.exe' -o -iname '*.dll' \) -print)
  (( ${#files[@]} == before )) && break
done
mapfile -t files < <(find dist -maxdepth 1 -type f \( -iname '*.exe' -o -iname '*.dll' \) -print | sort)
ntldd -R "${files[@]}" | tee dependencies-windows.txt
if awk 'BEGIN{IGNORECASE=1} /not found/{print}' dependencies-windows.txt | grep -Eiv '(api-ms-win-|ext-ms-win-|kernel32|user32|gdi32|shell32|advapi32|ole32|oleaut32|comdlg32|winmm|imm32|version|setupapi|ws2_32|ntdll|bcrypt|crypt32|secur32|dwmapi)'; then echo 'unresolved non-system DLL' >&2; exit 1; fi
cp "$font" dist/; cp packaging/licenses/DejaVu-FONT-LICENSE.txt dist/
sections=$(objdump -h dist/sudokura.exe); grep -Eq '[[:space:]]\.rsrc[[:space:]]' <<<"$sections"
headers=$(objdump -p dist/sudokura.exe); grep -q 'Subsystem.*Windows GUI' <<<"$headers"
resource_dump=$(objdump -s -j .rsrc dist/sudokura.exe); grep -qi '89504e47' <<<"$resource_dump"
SDL_VIDEODRIVER=dummy timeout 30s dist/sudokura.exe --smoke-test
find dist -type f -printf '%f\n' | sort | tee inventory-windows.txt
(cd dist && zip -9 ../Sudokura-v${VERSION}-windows-x86_64.zip ./*)
unzip -t "Sudokura-v${VERSION}-windows-x86_64.zip"; zip_inventory=$(unzip -Z1 "Sudokura-v${VERSION}-windows-x86_64.zip"); grep -q '^sudokura.exe$' <<<"$zip_inventory"
sha256sum "Sudokura-v${VERSION}-windows-x86_64.zip" > SHA256SUMS-windows.txt; du -h "Sudokura-v${VERSION}-windows-x86_64.zip"
