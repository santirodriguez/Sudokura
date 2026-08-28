#!/usr/bin/env bash
set -euo pipefail
SOURCE_VERSION=$(./scripts/version.sh)
if [[ -n "${VERSION:-}" && "$VERSION" != "$SOURCE_VERSION" ]]; then
  echo "VERSION=$VERSION does not match source version $SOURCE_VERSION" >&2
  exit 1
fi
VERSION=$SOURCE_VERSION

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

is_system_import() {
  local dll=$1 lower
  lower=$(printf '%s' "$dll" | tr '[:upper:]' '[:lower:]')
  case "$lower" in
    api-ms-*.dll|ext-ms-*.dll) return 0 ;;
  esac
  [[ -f "/c/Windows/System32/$dll" || -f "/c/Windows/SysWOW64/$dll" ]]
}

bundled_import_path() {
  find dist -maxdepth 1 -type f -iname "$1" -print -quit
}

mingw_import_path() {
  find /mingw64/bin -maxdepth 1 -type f -iname "$1" -print -quit
}

# Build the complete redistributable dependency closure from the direct PE
# import tables. This is independent of ntldd output formatting and never
# traverses optional internals of Windows system DLLs.
for pass in 1 2 3 4 5 6 7 8 9 10 11 12; do
  changed=0
  mapfile -t files < <(find dist -maxdepth 1 -type f \( -iname '*.exe' -o -iname '*.dll' \) -print | sort)
  test "${#files[@]}" -gt 0
  for file in "${files[@]}"; do
    while IFS= read -r dll; do
      test -n "$dll" || continue
      if [[ -n "$(bundled_import_path "$dll")" ]] || is_system_import "$dll"; then
        continue
      fi
      dep=$(mingw_import_path "$dll")
      if [[ -z "$dep" ]]; then
        printf '%s -> %s\n' "$file" "$dll" >&2
        echo 'direct DLL import is neither bundled, provided by Windows, nor available from MSYS2' >&2
        exit 1
      fi
      cp -n "$dep" dist/
      changed=1
    done < <(objdump -p "$file" | awk '/DLL Name:/{print $3}')
  done
  (( changed == 0 )) && break
done

mapfile -t files < <(find dist -maxdepth 1 -type f \( -iname '*.exe' -o -iname '*.dll' \) -print | sort)
test "${#files[@]}" -gt 0

# Verify closure independently after collection. Any remaining direct import
# outside the bundle, Windows, or an API-set contract is a real package error.
: > unresolved-direct-windows.txt
for file in "${files[@]}"; do
  while IFS= read -r dll; do
    test -n "$dll" || continue
    if [[ -n "$(bundled_import_path "$dll")" ]] || is_system_import "$dll"; then
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

# ntldd remains a human-readable report only. Prepending dist makes its search
# order match a portable launch; no direct dependency may resolve from MSYS2.
PATH="$PWD/dist:$PATH" ntldd "${files[@]}" | tee dependencies-windows.txt
if grep -Ei '=>[[:space:]]+[^[:space:]]*[/\\]mingw64[/\\]' dependencies-windows.txt; then
  echo 'packaged dependency still resolves from the MSYS2 installation' >&2
  exit 1
fi
PATH="$PWD/dist:$PATH" ntldd -R "${files[@]}" > dependencies-windows-recursive.txt

cp "$font" dist/
cp packaging/licenses/DejaVu-FONT-LICENSE.txt dist/

sections=$(objdump -h dist/sudokura.exe)
grep -Eq '[[:space:]]\.rsrc[[:space:]]' <<<"$sections"
headers=$(objdump -p dist/sudokura.exe)
grep -q 'Subsystem.*Windows GUI' <<<"$headers"
resource_dump=$(objdump -s -j .rsrc dist/sudokura.exe)
grep -qi '89504e47' <<<"$resource_dump"
strings -el dist/sudokura.exe | grep -Fxq "$VERSION"

timeout_bin=$(command -v timeout)
clean_path="$PWD/dist:/c/Windows/System32:/c/Windows"
env PATH="$clean_path" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  SDL_RENDER_DRIVER=software SDL_RENDER_VSYNC=0 \
  "$timeout_bin" 30s "$PWD/dist/sudokura.exe" --smoke-test

find dist -type f -printf '%f\n' | sort | tee inventory-windows.txt
archive="Sudokura-v${VERSION}-windows-x86_64.zip"
(cd dist && zip -9 "../$archive" ./*)
unzip -t "$archive"
zip_inventory=$(unzip -Z1 "$archive")
grep -q '^sudokura.exe$' <<<"$zip_inventory"

mkdir zipcheck
unzip -q "$archive" -d zipcheck
zip_clean_path="$PWD/zipcheck:/c/Windows/System32:/c/Windows"
env PATH="$zip_clean_path" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  SDL_RENDER_DRIVER=software SDL_RENDER_VSYNC=0 \
  "$timeout_bin" 30s "$PWD/zipcheck/sudokura.exe" --smoke-test

sha256sum "$archive" > SHA256SUMS-windows.txt
du -h "$archive"
