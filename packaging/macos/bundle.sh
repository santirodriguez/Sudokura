#!/usr/bin/env bash
set -euo pipefail
: "${PREFIX:?set PREFIX to the dependency prefix}"; APP="${1:-Sudokura.app}"; BIN="${2:-sudokura}"
rm -rf "$APP"; mkdir -p "$APP/Contents/"{MacOS,Frameworks,Resources}
cp "$BIN" "$APP/Contents/MacOS/sudokura"; cp packaging/macos/Info.plist "$APP/Contents/"
cp assets/generated/sudokura.icns "$APP/Contents/Resources/"
cp "${FONT:?set FONT to redistributable fallback font}" "$APP/Contents/Resources/DejaVuSans.ttf"
cp "${FONT_LICENSE:?set FONT_LICENSE to its license}" "$APP/Contents/Resources/DejaVu-FONT-LICENSE.txt"
queue=("$PREFIX/lib/libSDL2-2.0.0.dylib" "$PREFIX/lib/libSDL2_ttf-2.0.0.dylib")
index=0
while (( index < ${#queue[@]} )); do
 src=${queue[$index]}; ((index+=1))
 base=$(basename "$src"); [ -f "$APP/Contents/Frameworks/$base" ] && continue
 cp "$src" "$APP/Contents/Frameworks/$base"
 while read -r dep; do case "$dep" in "$PREFIX"/*) queue+=("$dep");; esac; done < <(otool -L "$src"|tail -n +2|awk '{print $1}')
done
if ! otool -l "$APP/Contents/MacOS/sudokura" | grep -q '@executable_path/../Frameworks'; then
  install_name_tool -add_rpath @executable_path/../Frameworks "$APP/Contents/MacOS/sudokura"
fi
for f in "$APP/Contents/MacOS/sudokura" "$APP/Contents/Frameworks/"*.dylib; do
 while read -r dep; do case "$dep" in "$PREFIX"/*) install_name_tool -change "$dep" "@rpath/$(basename "$dep")" "$f";; esac; done < <(otool -L "$f"|tail -n +2|awk '{print $1}')
done
for f in "$APP/Contents/Frameworks/"*.dylib; do install_name_tool -id "@rpath/$(basename "$f")" "$f"; done
dependencies=$(otool -L "$APP/Contents/MacOS/sudokura" "$APP/Contents/Frameworks/"*.dylib)
if grep -E '/opt/homebrew|/usr/local|/Users/runner' <<<"$dependencies"; then echo 'runner-local dependency detected' >&2; exit 1; fi
if awk '/^[[:space:]]+\//{print $1}' <<<"$dependencies" | grep -Ev '^(/usr/lib/|/System/Library/)'; then echo 'unbundled non-system dependency detected' >&2; exit 1; fi
find "$APP" -type f -print0|sort -z|xargs -0 shasum -a 256
du -sh "$APP"
