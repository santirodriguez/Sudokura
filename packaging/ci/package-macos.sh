#!/usr/bin/env bash
set -euo pipefail
: "${VERSION:?}" "${ARCH:?}" "${FONT:?}"
test -f "$FONT"; ./scripts/validate_font.py "$FONT"; export SUDOKURA_TEST_FONT="$FONT"
make assets; make WERROR=-Werror test test-ui all
FONT_LICENSE="$PWD/packaging/licenses/DejaVu-FONT-LICENSE.txt" PREFIX=$(brew --prefix) packaging/macos/bundle.sh Sudokura.app sudokura
file Sudokura.app/Contents/MacOS/sudokura | tee "architecture-macos-${ARCH}.txt"; grep -q "$ARCH" "architecture-macos-${ARCH}.txt"
for required in Contents/Info.plist Contents/Resources/sudokura.icns Contents/Resources/DejaVuSans.ttf Contents/Resources/DejaVu-FONT-LICENSE.txt; do test -s "Sudokura.app/$required"; done
./scripts/validate_font.py Sudokura.app/Contents/Resources/DejaVuSans.ttf
SDL_VIDEODRIVER=dummy python3 -c 'import subprocess; subprocess.run(["Sudokura.app/Contents/MacOS/sudokura","--smoke-test"],check=True,timeout=30)'
find Sudokura.app -type f | sort | tee "inventory-macos-${ARCH}.txt"
mapfile=(); while IFS= read -r -d '' f; do mapfile+=("$f"); done < <(find Sudokura.app/Contents/Frameworks -type f -name '*.dylib' -print0)
(( ${#mapfile[@]} > 0 )); otool -L Sudokura.app/Contents/MacOS/sudokura "${mapfile[@]}" | tee "dependencies-macos-${ARCH}.txt"
if grep -E '/opt/homebrew|/usr/local|/Users/runner|/private/var/folders' "dependencies-macos-${ARCH}.txt"; then echo 'build-host path remains' >&2; exit 1; fi
if awk '/^[[:space:]]+\//{print $1}' "dependencies-macos-${ARCH}.txt" | grep -Ev '^(/usr/lib/|/System/Library/)'; then echo 'unbundled non-system dylib' >&2; exit 1; fi
ditto -c -k --sequesterRsrc --keepParent Sudokura.app "Sudokura-v${VERSION}-macos-${ARCH}-unsigned.zip"
unzip -t "Sudokura-v${VERSION}-macos-${ARCH}-unsigned.zip"; zip_inventory=$(unzip -Z1 "Sudokura-v${VERSION}-macos-${ARCH}-unsigned.zip"); grep -q 'Sudokura.app/Contents/MacOS/sudokura' <<<"$zip_inventory"
shasum -a 256 "Sudokura-v${VERSION}-macos-${ARCH}-unsigned.zip" > "SHA256SUMS-macos-${ARCH}.txt"; du -h "Sudokura-v${VERSION}-macos-${ARCH}-unsigned.zip"
