#!/usr/bin/env bash
set -euo pipefail
SOURCE_VERSION=$(./scripts/version.sh)
if [[ -n "${VERSION:-}" && "$VERSION" != "$SOURCE_VERSION" ]]; then
  echo "VERSION=$VERSION does not match source version $SOURCE_VERSION" >&2
  exit 1
fi
VERSION=$SOURCE_VERSION
: "${INNO_ISCC:?set INNO_ISCC to the pinned Inno Setup compiler path}"

font=$(pacman -Ql mingw-w64-x86_64-ttf-dejavu | awk '!found && /\/DejaVuSans.ttf$/{value=$2;found=1} END{print value}')
test -n "$font"
test -f "$font"
./scripts/validate_font.py "$font"
export SUDOKURA_TEST_FONT="$font"

make assets
make WERROR=-Werror test test-ui
echo "generator benchmark host: $(uname -a)"
gcc --version | head -1
make WERROR=-Werror quality
windres packaging/windows/sudokura.rc -O coff -o icon.o
gcc -I. -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
  -Wformat-truncation=2 -Wstringop-truncation -Wformat-overflow=2 \
  sudokura_sdl.c app.c app_clock.c desktop.c audio.c profile.c save_policy.c store_io.c session.c seed.c progress.c input.c \
  game.c human.c geometry.c i18n.c \
  assets/generated/window_icon.c assets/generated/wordmark.c \
  assets/generated/flag_us.c assets/generated/flag_ar.c assets/generated/flag_ca.c icon.o \
  -o sudokura.exe $(pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_mixer) -lm -mwindows

rm -rf dist zipcheck installcheck
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

grep -qi 'SDL2_mixer' dependencies-windows.txt
cp "$font" dist/
cp packaging/licenses/DejaVu-FONT-LICENSE.txt dist/
cp LICENSE dist/LICENSE.txt
cp packaging/licenses/DISTRIBUTION-NOTICES.md dist/
cp packaging/windows/README.txt dist/README.txt
mkdir dist/audio
cp assets/audio/music-main.ogg dist/audio/
cp assets/audio/music-fail.ogg dist/audio/
cp assets/audio/jingle-win.ogg dist/audio/
cp assets/audio/jingle-fail.ogg dist/audio/
cp assets/audio/README.md dist/audio/PROVENANCE.md
for audio in music-main.ogg music-fail.ogg jingle-win.ogg jingle-fail.ogg; do test -s "dist/audio/$audio"; done

: > components-windows.txt
printf 'file\tpackage\tversion\tlicense_metadata\n' >> components-windows.txt
mkdir -p dist/licenses
for dll in dist/*.dll; do
  base=$(basename "$dll")
  source="/mingw64/bin/$base"
  if [[ -f "$source" ]]; then
    owner=$(pacman -Qo "$source")
    package=$(awk '{print $(NF-1)}' <<<"$owner")
    package_version=$(awk '{print $NF}' <<<"$owner")
    license_metadata=$(pacman -Qi "$package" | awk -F': +' '/^Licenses/{print $2; exit}')
    printf '%s\t%s\t%s\t%s\n' "$base" "$package" "$package_version" "$license_metadata" >> components-windows.txt
    while IFS= read -r license_file; do
      [[ -f "$license_file" ]] || continue
      install -Dm644 "$license_file" "dist/licenses/$package/$(basename "$license_file")"
    done < <(pacman -Ql "$package" | awk '/\/share\/licenses\//{print $2}')
  fi
done
sort -u components-windows.txt -o components-windows.txt
cp components-windows.txt dist/COMPONENTS.txt

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

find dist -type f -printf '%P\n' | sort | tee inventory-windows.txt
for audio in music-main.ogg music-fail.ogg jingle-win.ogg jingle-fail.ogg; do grep -Fxq "audio/$audio" inventory-windows.txt; done
archive="Sudokura-v${VERSION}-windows-x86_64.zip"
(cd dist && zip -9 -r "../$archive" ./*)
unzip -t "$archive"
zip_inventory=$(unzip -Z1 "$archive")
grep -q '^sudokura.exe$' <<<"$zip_inventory"
for audio in music-main.ogg music-fail.ogg jingle-win.ogg jingle-fail.ogg; do grep -Fxq "audio/$audio" <<<"$zip_inventory"; done

zip_extract="$PWD/zipcheck/Prueba con espacios á漢"
mkdir -p "$zip_extract"
unzip -q "$archive" -d "$zip_extract"
zip_clean_path="$zip_extract:/c/Windows/System32:/c/Windows"
env PATH="$zip_clean_path" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  SDL_RENDER_DRIVER=software SDL_RENDER_VSYNC=0 \
  "$timeout_bin" 30s "$zip_extract/sudokura.exe" --smoke-test

root_win=$(cygpath -w "$PWD")
export SUDOKURA_VERSION="$VERSION"
export SUDOKURA_ROOT_WIN="$root_win"
"$INNO_ISCC" packaging/windows/sudokura.iss
installer="Sudokura-v${VERSION}-windows-x86_64-setup.exe"
test -s "$installer"
installer_product=$(powershell.exe -NoProfile -Command "(Get-Item '$(cygpath -w "$PWD/$installer")').VersionInfo.ProductVersion" | tr -d '\r')
grep -Fq "$VERSION" <<<"$installer_product"

install_dir="$PWD/installcheck/Sudokura con espacios á漢"
mkdir -p "$(dirname "$install_dir")"

# The installer owns application files only. SDL_GetPrefPath keeps saves and
# settings in the roaming per-user profile; prove maintenance reinstall and
# uninstall do not erase that profile before Phase 8 performs the real v1.2
# upgrade acceptance on a clean Windows environment.
profile_dir="$(cygpath -u "$APPDATA")/santirodriguez/Sudokura"
mkdir -p "$profile_dir"
profile_sentinel="$profile_dir/phase7-installer-profile-sentinel.txt"
printf 'preserve-user-profile\n' > "$profile_sentinel"

"$PWD/$installer" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /CURRENTUSER "/DIR=$(cygpath -w "$install_dir")"
test -s "$install_dir/sudokura.exe"
test -s "$install_dir/README.txt"
test -s "$profile_sentinel"
installed_path="$install_dir:/c/Windows/System32:/c/Windows"
env PATH="$installed_path" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  SDL_RENDER_DRIVER=software SDL_RENDER_VSYNC=0 \
  "$timeout_bin" 30s "$install_dir/sudokura.exe" --smoke-test

# Exercise Inno's existing-AppId maintenance/update path without pretending it
# substitutes for the real v1.2 -> v1.3 acceptance required in Phase 8.
"$PWD/$installer" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- /CURRENTUSER "/DIR=$(cygpath -w "$install_dir")"
test -s "$install_dir/sudokura.exe"
test -s "$profile_sentinel"

test -s "$install_dir/unins000.exe"
"$install_dir/unins000.exe" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
test ! -e "$install_dir/sudokura.exe"
test -s "$profile_sentinel"
rm -f "$profile_sentinel"

sha256sum "$archive" "$installer" > SHA256SUMS-windows.txt
./packaging/ci/write-build-provenance.sh windows build-provenance-windows.txt
python3 scripts/write_artifact_manifest.py \
  --platform windows --architecture x86_64 \
  --minimum 'Windows 11 x64 candidate support target; Windows 10 x64 pending Phase 8 validation' \
  --kind "${SUDOKURA_ARTIFACT_KIND:-candidate}" \
  --output artifact-manifest-windows.json \
  --artifact "$archive" --baseline 9406901 \
  --artifact "$installer" --baseline 0
du -h "$archive" "$installer"
