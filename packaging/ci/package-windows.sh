#!/usr/bin/env bash
set -euo pipefail
SOURCE_VERSION=$(./scripts/version.sh)
if [[ -n "${VERSION:-}" && "$VERSION" != "$SOURCE_VERSION" ]]; then
  echo "VERSION=$VERSION does not match source version $SOURCE_VERSION" >&2
  exit 1
fi
VERSION=$SOURCE_VERSION
SOURCE_COMMIT=$(./scripts/source_commit.sh)
export SUDOKURA_SOURCE_COMMIT="$SOURCE_COMMIT"
: "${INNO_ISCC:?set INNO_ISCC to the pinned Inno Setup compiler path}"
: "${NSIS_MAKENSIS:?set NSIS_MAKENSIS to the pinned NSIS compiler path}"

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
gcc -I. -Isrc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror \
  -Wformat-truncation=2 -Wstringop-truncation -Wformat-overflow=2 \
  src/sudokura_sdl.c src/app.c src/app_clock.c src/desktop.c src/url_launcher.c src/audio.c src/profile.c src/save_policy.c src/store_io.c src/session.c src/seed.c src/progress.c src/input.c \
  src/game.c src/human.c src/geometry.c src/i18n.c \
  assets/generated/window_icon.c assets/generated/wordmark.c \
  assets/generated/flag_us.c assets/generated/flag_ar.c assets/generated/flag_ca.c icon.o \
  -o sudokura.exe $(pkg-config --cflags --libs sdl2 SDL2_ttf SDL2_mixer) -lshell32 -lm -mwindows

rm -rf dist zipcheck installcheck portablecheck portabletmp portable-fail-probe.exe
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
archive="Sudokura-${VERSION}-Windows-x64-Internal.zip"
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

portable="Sudokura-${VERSION}-Windows-x64-Portable.exe"
root_win=$(cygpath -w "$PWD")
dist_win=$(cygpath -w "$PWD/dist")
portable_win=$(cygpath -w "$PWD/$portable")
portable_script_win=$(cygpath -w "$PWD/packaging/windows/sudokura-portable.nsi")
env MSYS2_ARG_CONV_EXCL='*' "$NSIS_MAKENSIS" \
  "/DVERSION=$VERSION" \
  "/DROOT=$root_win" \
  "/DDIST=$dist_win" \
  "/DOUTPUT=$portable_win" \
  "$portable_script_win"
test -s "$portable"

portable_product=$(powershell.exe -NoProfile -Command "(Get-Item '$(cygpath -w "$PWD/$portable")').VersionInfo.ProductVersion" | tr -d '\r')
grep -Fq "$VERSION" <<<"$portable_product"
portable_headers=$(objdump -p "$portable")
grep -q 'Subsystem.*Windows GUI' <<<"$portable_headers"

portable_dir="$PWD/portablecheck/Prueba con espacios á漢"
portable_temp="$PWD/portabletmp/normal"
mkdir -p "$portable_dir" "$portable_temp"
portable_copy="$portable_dir/$portable"
font_probe="$portable_dir/Fuente con espacios á漢.ttf"
cp "$portable" "$portable_copy"
cp "$font" "$font_probe"
portable_temp_win=$(cygpath -w "$portable_temp")
font_probe_win=$(cygpath -w "$font_probe")
portable_clean_path="/c/Windows/System32:/c/Windows"

# A quoted Unicode argument and the launcher path itself both contain spaces
# and non-ASCII characters. --smoke-test must reach the contained game or this
# command will not terminate successfully.
env TEMP="$portable_temp_win" TMP="$portable_temp_win" PATH="$portable_clean_path" \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_RENDER_DRIVER=software SDL_RENDER_VSYNC=0 \
  "$timeout_bin" 30s "$portable_copy" --font "$font_probe_win" --smoke-test
test -z "$(find "$portable_temp" -mindepth 1 -print -quit)"

# The launcher's process exit code must be the contained game's exit code.
child_fail_temp="$PWD/portabletmp/child-failure"
mkdir -p "$child_fail_temp"
child_fail_temp_win=$(cygpath -w "$child_fail_temp")
set +e
env TEMP="$child_fail_temp_win" TMP="$child_fail_temp_win" PATH="$portable_clean_path" \
  SDL_VIDEODRIVER=sudokura-no-video-device SDL_AUDIODRIVER=dummy \
  "$timeout_bin" 30s "$portable_copy" --smoke-test
child_fail_status=$?
set -e
test "$child_fail_status" -eq 1
test -z "$(find "$child_fail_temp" -mindepth 1 -print -quit)"

# Compile a non-shipping probe from the same launcher source to force a direct
# child-launch failure. It must fail visibly and still clean its private temp.
fail_probe="$PWD/portable-fail-probe.exe"
fail_probe_win=$(cygpath -w "$fail_probe")
env MSYS2_ARG_CONV_EXCL='*' "$NSIS_MAKENSIS" \
  "/DVERSION=$VERSION" \
  "/DROOT=$root_win" \
  "/DDIST=$dist_win" \
  "/DOUTPUT=$fail_probe_win" \
  "/DSUDOKURA_PORTABLE_TEST_FAIL_LAUNCH=1" \
  "$portable_script_win"
fail_temp="$PWD/portabletmp/launch-failure"
mkdir -p "$fail_temp"
fail_temp_win=$(cygpath -w "$fail_temp")
set +e
env TEMP="$fail_temp_win" TMP="$fail_temp_win" PATH="$portable_clean_path" \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  "$timeout_bin" 30s "$fail_probe"
fail_status=$?
set -e
test "$fail_status" -eq 127
test -z "$(find "$fail_temp" -mindepth 1 -print -quit)"
rm -f "$fail_probe"

# Exercise several launchers against one TEMP root. Each NSIS instance owns a
# separate $PLUGINSDIR; successful exits must leave the isolated root empty.
concurrent_temp="$PWD/portabletmp/concurrent"
mkdir -p "$concurrent_temp"
concurrent_temp_win=$(cygpath -w "$concurrent_temp")
pids=()
for i in 1 2 3; do
  env TEMP="$concurrent_temp_win" TMP="$concurrent_temp_win" PATH="$portable_clean_path" \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_RENDER_DRIVER=software SDL_RENDER_VSYNC=0 \
    "$timeout_bin" 30s "$portable_copy" --smoke-test &
  pids+=("$!")
done
for pid in "${pids[@]}"; do wait "$pid"; done
test -z "$(find "$concurrent_temp" -mindepth 1 -print -quit)"

# Record same-run size and cold/warm smoke evidence. These are measurements,
# not universal performance promises.
benchmark="portable-benchmark-windows.txt"
: > "$benchmark"
printf 'zip_bytes=%s\n' "$(stat -c%s "$archive")" >> "$benchmark"
printf 'portable_bytes=%s\n' "$(stat -c%s "$portable")" >> "$benchmark"

# Forced termination is intentionally tested in an isolated TEMP root. The
# launcher waits for the child, so killing the process tree can interrupt normal
# cleanup. Record whether residue remains, then remove only this test root.
interrupt_temp="$PWD/portabletmp/forced-interrupt"
mkdir -p "$interrupt_temp"
interrupt_temp_win=$(cygpath -w "$interrupt_temp")
portable_copy_win=$(cygpath -w "$portable_copy")
powershell.exe -NoProfile -Command "\$env:TEMP='$interrupt_temp_win'; \$env:TMP='$interrupt_temp_win'; \$env:SDL_VIDEODRIVER='dummy'; \$env:SDL_AUDIODRIVER='dummy'; \$env:SDL_RENDER_DRIVER='software'; \$env:SDL_RENDER_VSYNC='0'; \$p=Start-Process -FilePath '$portable_copy_win' -PassThru; Start-Sleep -Seconds 2; if (-not \$p.HasExited) { & taskkill.exe /PID \$p.Id /T /F | Out-Null }; exit 0"
sleep 1
if [[ -n "$(find "$interrupt_temp" -mindepth 1 -print -quit)" ]]; then
  printf 'forced_interrupt_residual=present\n' >> "$benchmark"
else
  printf 'forced_interrupt_residual=none\n' >> "$benchmark"
fi
printf 'forced_interrupt_cleanup_guarantee=none\n' >> "$benchmark"
rm -rf "$interrupt_temp"

zip_start=$(date +%s%3N)
env PATH="$zip_clean_path" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  SDL_RENDER_DRIVER=software SDL_RENDER_VSYNC=0 \
  "$timeout_bin" 30s "$zip_extract/sudokura.exe" --smoke-test
zip_end=$(date +%s%3N)
printf 'zip_smoke_ms=%s\n' "$((zip_end-zip_start))" >> "$benchmark"
measure_smoke_ms() {
  local label=$1 exe=$2 temp_dir=$3
  shift 3
  rm -rf "$temp_dir"
  mkdir -p "$temp_dir"
  local temp_win start end
  temp_win=$(cygpath -w "$temp_dir")
  start=$(date +%s%3N)
  env TEMP="$temp_win" TMP="$temp_win" PATH="$portable_clean_path" \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_RENDER_DRIVER=software SDL_RENDER_VSYNC=0 \
    "$timeout_bin" 30s "$exe" "$@"
  end=$(date +%s%3N)
  printf '%s_ms=%s\n' "$label" "$((end-start))" >> "$benchmark"
  test -z "$(find "$temp_dir" -mindepth 1 -print -quit)"
}
measure_smoke_ms portable_cold "$portable_copy" "$PWD/portabletmp/bench-cold" --smoke-test
measure_smoke_ms portable_warm "$portable_copy" "$PWD/portabletmp/bench-warm" --smoke-test

export SUDOKURA_VERSION="$VERSION"
export SUDOKURA_ROOT_WIN="$root_win"
"$INNO_ISCC" packaging/windows/sudokura.iss
installer="Sudokura-${VERSION}-Windows-x64-Setup.exe"
test -s "$installer"
printf 'installer_bytes=%s\n' "$(stat -c%s "$installer")" >> "$benchmark"
installer_product=$(powershell.exe -NoProfile -Command "(Get-Item '$(cygpath -w "$PWD/$installer")').VersionInfo.ProductVersion" | tr -d '\r')
grep -Fq "$VERSION" <<<"$installer_product"

install_dir="$PWD/installcheck/Sudokura con espacios á漢"
mkdir -p "$(dirname "$install_dir")"

# The installer owns application files only. SDL_GetPrefPath keeps saves and
# settings in the roaming per-user profile; prove maintenance reinstall and
# uninstall do not erase that profile. The final v1.2 -> v1.3 upgrade path
# was separately accepted on a real Windows environment.
profile_dir="$(cygpath -u "$APPDATA")/santirodriguez/Sudokura"
mkdir -p "$profile_dir"
profile_sentinel="$profile_dir/phase7-installer-profile-sentinel.txt"
printf 'preserve-user-profile\n' > "$profile_sentinel"

env MSYS2_ARG_CONV_EXCL='*' "$timeout_bin" 120s "$PWD/$installer" \
  /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- \
  "/DIR=$(cygpath -w "$install_dir")"
test -s "$install_dir/sudokura.exe"
test -s "$install_dir/README.txt"
test -s "$profile_sentinel"
installed_path="$install_dir:/c/Windows/System32:/c/Windows"
installed_start=$(date +%s%3N)
env PATH="$installed_path" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  SDL_RENDER_DRIVER=software SDL_RENDER_VSYNC=0 \
  "$timeout_bin" 30s "$install_dir/sudokura.exe" --smoke-test
installed_end=$(date +%s%3N)
printf 'installed_smoke_ms=%s\n' "$((installed_end-installed_start))" >> "$benchmark"

# Package switching must not move or erase the shared AppData profile.
portable_switch_temp="$PWD/portabletmp/package-switch"
mkdir -p "$portable_switch_temp"
portable_switch_temp_win=$(cygpath -w "$portable_switch_temp")
env TEMP="$portable_switch_temp_win" TMP="$portable_switch_temp_win" PATH="$portable_clean_path" \
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_RENDER_DRIVER=software SDL_RENDER_VSYNC=0 \
  "$timeout_bin" 30s "$portable_copy" --smoke-test
test -s "$profile_sentinel"
test -z "$(find "$portable_switch_temp" -mindepth 1 -print -quit)"

# Exercise Inno's existing-AppId maintenance/update path in CI. The real
# v1.2 -> v1.3 upgrade path was separately accepted on Windows.
env MSYS2_ARG_CONV_EXCL='*' "$timeout_bin" 120s "$PWD/$installer" \
  /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP- \
  "/DIR=$(cygpath -w "$install_dir")"
test -s "$install_dir/sudokura.exe"
test -s "$profile_sentinel"

test -s "$install_dir/unins000.exe"
env MSYS2_ARG_CONV_EXCL='*' "$timeout_bin" 120s "$install_dir/unins000.exe" \
  /VERYSILENT /SUPPRESSMSGBOXES /NORESTART
test ! -e "$install_dir/sudokura.exe"
test -s "$profile_sentinel"
rm -f "$profile_sentinel"

sha256sum "$portable" "$installer" > SHA256SUMS-windows.txt
sha256sum "$archive" > internal-zip-sha256-windows.txt
./packaging/ci/write-build-provenance.sh windows build-provenance-windows.txt
python3 scripts/write_artifact_manifest.py \
  --version "$VERSION" --source-commit "$SOURCE_COMMIT" \
  --platform windows --architecture x86_64 \
  --minimum 'Windows 11 x64 support target; final real Windows acceptance passed for v1.3.0; Windows 10 x64 is not claimed' \
  --kind "${SUDOKURA_ARTIFACT_KIND:-candidate}" \
  --output artifact-manifest-windows.json \
  --artifact "$portable" --baseline 0 \
  --artifact "$installer" --baseline 0
cat "$benchmark"
du -h "$archive" "$portable" "$installer"
