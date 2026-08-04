#!/usr/bin/env bash
set -euo pipefail
: "${VERSION:?}" "${LINUXDEPLOY:?path to linuxdeploy}"
font=$(fc-match -f '%{file}' 'DejaVu Sans'); test -f "$font"; ./scripts/validate_font.py "$font"
rm -rf AppDir; install -Dm755 sudokura AppDir/usr/bin/sudokura
install -Dm644 packaging/linux/sudokura.desktop AppDir/usr/share/applications/sudokura.desktop
install -Dm644 assets/generated/sudokura-256.png AppDir/usr/share/icons/hicolor/256x256/apps/sudokura.png
install -Dm644 "$font" AppDir/usr/bin/DejaVuSans.ttf
install -Dm644 packaging/licenses/DejaVu-FONT-LICENSE.txt AppDir/usr/bin/DejaVu-FONT-LICENSE.txt
SDL_VIDEODRIVER=dummy timeout 30s AppDir/usr/bin/sudokura --smoke-test
"$LINUXDEPLOY" --appdir AppDir --output appimage
mv Sudokura*.AppImage "Sudokura-v${VERSION}-linux-x86_64.AppImage"; chmod +x "Sudokura-v${VERSION}-linux-x86_64.AppImage"
test -x "Sudokura-v${VERSION}-linux-x86_64.AppImage"
SDL_VIDEODRIVER=dummy APPIMAGE_EXTRACT_AND_RUN=1 timeout 30s "./Sudokura-v${VERSION}-linux-x86_64.AppImage" --smoke-test
find AppDir -type f -printf '%P\n' | sort > inventory-linux.txt
ldd AppDir/usr/bin/sudokura | tee dependencies-linux.txt
sha256sum "Sudokura-v${VERSION}-linux-x86_64.AppImage" > SHA256SUMS-linux.txt
du -h "Sudokura-v${VERSION}-linux-x86_64.AppImage"
