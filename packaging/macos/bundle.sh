#!/usr/bin/env bash
set -euo pipefail
: "${PREFIX:?set PREFIX to the dependency prefix}"
: "${FONT:?set FONT to redistributable fallback font}"
: "${FONT_LICENSE:?set FONT_LICENSE to its license}"
: "${ARCH:?set ARCH}"
: "${MIN_MACOS:?set MIN_MACOS}"

if [[ "$ARCH" != arm64 ]]; then
  echo "v1.3 macOS candidate is arm64-only" >&2
  exit 1
fi

APP="${1:-Sudokura.app}"
BIN="${2:-sudokura}"
VERSION=$(./scripts/version.sh)
components="components-macos-${ARCH}.txt"
printf 'file\tformula\tversion\tlicense_metadata\n' > "$components"

rm -rf "$APP"
mkdir -p "$APP/Contents/"{MacOS,Frameworks,Resources/audio,Resources/Documentation/licenses}
cp "$BIN" "$APP/Contents/MacOS/sudokura"
sed -e "s/@SUDOKURA_VERSION@/$VERSION/g" -e "s/@SUDOKURA_MIN_MACOS@/$MIN_MACOS/g" \
  packaging/macos/Info.plist > "$APP/Contents/Info.plist"
cp assets/generated/sudokura.icns "$APP/Contents/Resources/"
cp "$FONT" "$APP/Contents/Resources/DejaVuSans.ttf"
cp "$FONT_LICENSE" "$APP/Contents/Resources/DejaVu-FONT-LICENSE.txt"
cp assets/audio/music-main.ogg "$APP/Contents/Resources/audio/"
cp assets/audio/music-fail.ogg "$APP/Contents/Resources/audio/"
cp assets/audio/jingle-win.ogg "$APP/Contents/Resources/audio/"
cp assets/audio/jingle-fail.ogg "$APP/Contents/Resources/audio/"
cp LICENSE "$APP/Contents/Resources/Documentation/LICENSE.txt"
cp packaging/licenses/DISTRIBUTION-NOTICES.md "$APP/Contents/Resources/Documentation/"
cp assets/audio/README.md "$APP/Contents/Resources/Documentation/AUDIO-PROVENANCE.md"

cellar=$(brew --cellar)
record_component() {
  local bundled_name=$1 source_path=$2 real rel formula rest formula_version license formula_prefix dest found
  real=$(python3 - "$source_path" <<'PY'
import os, sys
print(os.path.realpath(sys.argv[1]))
PY
)
  formula=unknown
  formula_version=unknown
  license=unknown
  if [[ "$real" == "$cellar/"* ]]; then
    rel=${real#"$cellar/"}
    formula=${rel%%/*}
    rest=${rel#*/}
    formula_version=${rest%%/*}
    license=$(brew info --json=v2 "$formula" | python3 -c 'import json,sys; data=json.load(sys.stdin)["formulae"][0]; print(data.get("license") or "UNKNOWN")')
    dest="$APP/Contents/Resources/Documentation/licenses/$formula"
    if [[ ! -e "$dest/.collected" ]]; then
      mkdir -p "$dest"
      formula_prefix=$(brew --prefix "$formula")
      found=0
      while IFS= read -r license_file; do
        [[ -f "$license_file" ]] || continue
        cp "$license_file" "$dest/$(basename "$license_file")"
        found=1
      done < <(find "$formula_prefix" -type f \( -iname 'LICENSE*' -o -iname 'COPYING*' -o -iname 'COPYRIGHT*' -o -iname 'NOTICE*' \) -print 2>/dev/null)
      if [[ "$found" == 0 ]]; then
        printf 'Homebrew formula license metadata: %s\n' "$license" > "$dest/LICENSE-METADATA.txt"
      fi
      : > "$dest/.collected"
    fi
  fi
  printf '%s\t%s\t%s\t%s\n' "$bundled_name" "$formula" "$formula_version" "$license" >> "$components"
}

queue=("$PREFIX/lib/libSDL2-2.0.0.dylib" "$PREFIX/lib/libSDL2_ttf-2.0.0.dylib" "$PREFIX/lib/libSDL2_mixer-2.0.0.dylib")
index=0
while (( index < ${#queue[@]} )); do
  src=${queue[$index]}
  ((index+=1))
  test -f "$src"
  base=$(basename "$src")
  [[ -f "$APP/Contents/Frameworks/$base" ]] && continue
  cp "$src" "$APP/Contents/Frameworks/$base"
  record_component "$base" "$src"
  while read -r dep; do
    case "$dep" in
      "$PREFIX"/*) queue+=("$dep") ;;
    esac
  done < <(otool -L "$src" | tail -n +2 | awk '{print $1}')
done

{
  head -1 "$components"
  tail -n +2 "$components" | LC_ALL=C sort -u
} > "$components.tmp"
mv "$components.tmp" "$components"
find "$APP/Contents/Resources/Documentation/licenses" -name .collected -delete
cp "$components" "$APP/Contents/Resources/Documentation/COMPONENTS.txt"

frameworks=("$APP/Contents/Frameworks/"*.dylib)
(( ${#frameworks[@]} > 0 ))

for file_path in "$APP/Contents/MacOS/sudokura" "${frameworks[@]}"; do
  while read -r rpath; do
    case "$rpath" in
      "$PREFIX"/*|/Users/*|/private/var/folders/*)
        install_name_tool -delete_rpath "$rpath" "$file_path"
        ;;
    esac
  done < <(otool -l "$file_path" | awk '/LC_RPATH/{getline;getline;print $2}')
done

load_commands=$(otool -l "$APP/Contents/MacOS/sudokura")
if ! grep -q '@executable_path/../Frameworks' <<<"$load_commands"; then
  install_name_tool -add_rpath @executable_path/../Frameworks "$APP/Contents/MacOS/sudokura"
fi

for file_path in "$APP/Contents/MacOS/sudokura" "${frameworks[@]}"; do
  while read -r dep; do
    case "$dep" in
      "$PREFIX"/*)
        install_name_tool -change "$dep" "@rpath/$(basename "$dep")" "$file_path"
        ;;
    esac
  done < <(otool -L "$file_path" | tail -n +2 | awk '{print $1}')
done
for file_path in "${frameworks[@]}"; do
  install_name_tool -id "@rpath/$(basename "$file_path")" "$file_path"
done

dependencies=$(otool -L "$APP/Contents/MacOS/sudokura" "${frameworks[@]}")
load_commands=$(otool -l "$APP/Contents/MacOS/sudokura" "${frameworks[@]}")
if grep -E '/opt/homebrew|/usr/local|/Users/|/private/var/folders' <<<"$dependencies$load_commands"; then
  echo 'build-host dependency or rpath detected' >&2
  exit 1
fi
if awk '/^[[:space:]]+\//{print $1}' <<<"$dependencies" | grep -Ev '^(/usr/lib/|/System/Library/)'; then
  echo 'unbundled non-system dependency detected' >&2
  exit 1
fi

for audio in music-main.ogg music-fail.ogg jingle-win.ogg jingle-fail.ogg; do
  test -s "$APP/Contents/Resources/audio/$audio"
done

xattr -cr "$APP"
for file_path in "${frameworks[@]}"; do
  codesign --force --sign - "$file_path"
done
codesign --force --sign - "$APP/Contents/MacOS/sudokura"
codesign --force --sign - "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"

find "$APP" -type f -print | LC_ALL=C sort | while IFS= read -r file_path; do
  shasum -a 256 "$file_path"
done
du -sh "$APP"
