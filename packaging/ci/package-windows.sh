#!/usr/bin/env bash
set -euo pipefail
: "${VERSION:?}"

font=$(pacman -Ql mingw-w64-x86_64-ttf-dejavu | awk '!found && /\/DejaVuSans.ttf$/{value=$2;found=1} END{print value}')
test -n "$font"
test -f "$font"
./scripts/validate_font.py "$font"
export SUDOKURA_TEST_FONT="$font"

make assets
make WERROR=-Werror test test-ui
windres packaging/windows/sudokura.rc -O coff -o icon.o
gcc -I. -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
  -Wformat-truncation=2 -Wstringop-truncation -Wformat-overflow=2 \
  sudokura_sdl.c game.c geometry.c i18n.c \
  assets/generated/window_icon.c assets/generated/wordmark.c icon.o \
  -o sudokura.exe $(pkg-config --cflags --libs sdl2 SDL2_ttf) -lm -mwindows

rm -rf dist zipcheck
mkdir dist
cp sudokura.exe dist/

# Recursively discover only redistributable MSYS2 dependencies. ntldd -R also
# descends into Windows system DLLs, whose optional/API-set imports are useful
# diagnostics but are not files that belong in a portable application bundle.
for pass in 1 2 3 4 5 6; do
  mapfile -t files < <(find dist -maxdepth 1 -type f \( -iname '*.exe' -o -iname '*.dll' \) -print | sort)
  test "${#files[@]}" -gt 0
  ntldd -R "${files[@]}" > dependencies-windows-recursive.txt
  mapfile -t deps < <(awk 'tolower($0) ~ /=> \/mingw64\// {print $3}' dependencies-windows-recursive.txt | sort -u)
  before=${#files[@]}
  for dep in "${deps[@]}"; do
    test -f "$dep"
    cp -n "$dep" dist/
  done
  mapfile -t files < <(find dist -maxdepth 1 -type f \( -iname '*.exe' -o -iname '*.dll' \) -print)
  (( ${#files[@]} == before )) && break
done

mapfile -t files < <(find dist -maxdepth 1 -type f \( -iname '*.exe' -o -iname '*.dll' \) -print | sort)
test "${#files[@]}" -gt 0

# Keep a concise direct-dependency report. Recursive system-DLL traversal is
# intentionally not used as a release gate because it reports optional Windows
# internals and API-set contracts that applications neither ship nor control.
ntldd "${files[@]}" | tee dependencies-windows.txt

if grep -Ei '=>[[:space:]]+(/mingw64/|[A-Za-z]:[\\/][^[:space:]]*mingw64[\\/])' dependencies-windows.txt; then
  echo 'packaged dependency still resolves from the MSYS2 installation' >&2
  exit 1
fi

# Validate the actual PE import tables of every file we ship. An import is
# satisfied only by another bundled file, a physical Windows system DLL, or a
# Windows API-set contract. Any other direct import is a real packaging error.
: > unresolved-direct-windows.txt
for file in "${files[@]}"; do
  while IFS= read -r dll; do
    test -n "$dll" || continue
    lower=$(printf '%s' "$dll" | tr '[:upper:]' '[:lower:]')
    case "$lower" in
      api-ms-*.dll|ext-ms-*.dll) continue ;;
    esac
    if find dist -maxdepth 1 -type f -iname "$dll" -print -quit | grep -q .; then
      continue
    fi
    if [[ -f "/c/Windows/System32/$dll" || -f "/c/Windows/SysWOW64/$dll" ]]; then
      continue
    fi
    printf '%s -> %s\n' "$file" "$dll" >> unresolved-direct-windows.txt
  done < <(objdump -p "$file" | awk '/DLL Name:/{print $3}')
done

if [[ -s unresolved-direct-windows.txt ]]; then
  cat unresolved-direct-windows.txt >&2
  echo 'unresolved direct non-system DLL import' >&2
  exit 1
fi

cp "$font" dist/
cp packaging/licenses/DejaVu-FONT-LICENSE.txt dist/

sections=$(objdump -h dist/sudokura.exe)
grep -Eq '[[:space:]]\.rsrc[[:space:]]' <<<"$sections"
headers=$(objdump -p dist/sudokura.exe)
grep -q 'Subsystem.*Windows GUI' <<<"$headers"
resource_dump=$(objdump -s -j .rsrc dist/sudokura.exe)
grep -qi '89504e47' <<<"$resource_dump"

# Use a clean runtime PATH so the MSYS2 installation cannot hide a DLL that is
# absent from the portable directory.
timeout_bin=$(command -v timeout)
clean_path="$PWD/dist:/c/Windows/System32:/c/Windows"
env PATH="$clean_path" SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software SDL_RENDER_VSYNC=0 \
  "$timeout_bin" 30s "$PWD/dist/sudokura.exe" --smoke-test

find dist -type f -printf '%f\n' | sort | tee inventory-windows.txt
archive="Sudokura-v${VERSION}-windows-x86_64.zip"
(cd dist && zip -9 "../$archive" ./*)
unzip -t "$archive"
zip_inventory=$(unzip -Z1 "$archive")
grep -q '^sudokura.exe$' <<<"$zip_inventory"

# Validate the exact extracted archive, again without access to MSYS2 DLLs.
mkdir zipcheck
unzip -q "$archive" -d zipcheck
zip_clean_path="$PWD/zipcheck:/c/Windows/System32:/c/Windows"
env PATH="$zip_clean_path" SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software SDL_RENDER_VSYNC=0 \
  "$timeout_bin" 30s "$PWD/zipcheck/sudokura.exe" --smoke-test

sha256sum "$archive" > SHA256SUMS-windows.txt
du -h "$archive"
