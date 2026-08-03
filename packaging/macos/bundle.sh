#!/usr/bin/env bash
set -euo pipefail
: "${PREFIX:?set PREFIX to the dependency prefix}"; APP="${1:-Sudokura.app}"; BIN="${2:-sudokura}"
rm -rf "$APP"; mkdir -p "$APP/Contents/"{MacOS,Frameworks,Resources}
cp "$BIN" "$APP/Contents/MacOS/sudokura"; cp packaging/macos/Info.plist "$APP/Contents/"
cp assets/generated/sudokura.icns "$APP/Contents/Resources/"; cp "${FONT:?set FONT to fallback font}" "$APP/Contents/Resources/DejaVuSans.ttf"
queue=("$PREFIX/lib/libSDL2-2.0.0.dylib" "$PREFIX/lib/libSDL2_ttf-2.0.0.dylib")
for src in "${queue[@]}"; do
 base=$(basename "$src"); [ -f "$APP/Contents/Frameworks/$base" ] && continue
 cp "$src" "$APP/Contents/Frameworks/$base"
 while read -r dep; do case "$dep" in "$PREFIX"/*) queue+=("$dep");; esac; done < <(otool -L "$src"|tail -n +2|awk '{print $1}')
done
install_name_tool -add_rpath @executable_path/../Frameworks "$APP/Contents/MacOS/sudokura" 2>/dev/null || true
for f in "$APP/Contents/MacOS/sudokura" "$APP/Contents/Frameworks/"*.dylib; do
 while read -r dep; do case "$dep" in "$PREFIX"/*) install_name_tool -change "$dep" "@rpath/$(basename "$dep")" "$f";; esac; done < <(otool -L "$f"|tail -n +2|awk '{print $1}')
done
for f in "$APP/Contents/Frameworks/"*.dylib; do install_name_tool -id "@rpath/$(basename "$f")" "$f"; done
if otool -L "$APP/Contents/MacOS/sudokura" "$APP/Contents/Frameworks/"*.dylib | grep -E '/opt/homebrew|/usr/local|/Users/runner'; then echo 'runner-local dependency detected' >&2; exit 1; fi
find "$APP" -type f -print0|sort -z|xargs -0 shasum -a 256
du -sh "$APP"
