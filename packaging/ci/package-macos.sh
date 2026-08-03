#!/usr/bin/env bash
set -euo pipefail
: "${VERSION:?}" "${ARCH:?}" "${FONT:?}"
make assets; make WERROR=-Werror test all
FONT_LICENSE="$PWD/packaging/licenses/DejaVu-FONT-LICENSE.txt" PREFIX=$(brew --prefix) packaging/macos/bundle.sh Sudokura.app sudokura
file Sudokura.app/Contents/MacOS/sudokura | tee architecture-macos-${ARCH}.txt; grep -q "$ARCH" architecture-macos-${ARCH}.txt
test -f Sudokura.app/Contents/Info.plist; test -f Sudokura.app/Contents/Resources/sudokura.icns; test -f Sudokura.app/Contents/Resources/DejaVuSans.ttf
SDL_VIDEODRIVER=dummy Sudokura.app/Contents/MacOS/sudokura --smoke-test
find Sudokura.app -type f | sort | tee inventory-macos-${ARCH}.txt
otool -L Sudokura.app/Contents/MacOS/sudokura Sudokura.app/Contents/Frameworks/*.dylib | tee dependencies-macos-${ARCH}.txt
ditto -c -k --sequesterRsrc --keepParent Sudokura.app "Sudokura-v${VERSION}-macos-${ARCH}-unsigned.zip"
unzip -t "Sudokura-v${VERSION}-macos-${ARCH}-unsigned.zip"; shasum -a 256 Sudokura-*.zip > "SHA256SUMS-macos-${ARCH}.txt"; du -h Sudokura-*.zip
