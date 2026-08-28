#!/usr/bin/env bash
set -euo pipefail
: "${LINUXDEPLOY:?path to linuxdeploy}"
SOURCE_VERSION=$(./scripts/version.sh)
if [[ -n "${VERSION:-}" && "$VERSION" != "$SOURCE_VERSION" ]]; then
  echo "VERSION=$VERSION does not match source version $SOURCE_VERSION" >&2
  exit 1
fi
VERSION=$SOURCE_VERSION
font=$(fc-match -f '%{file}' 'DejaVu Sans'); test -f "$font"; ./scripts/validate_font.py "$font"
rm -rf AppDir; install -Dm755 sudokura AppDir/usr/bin/sudokura
install -Dm644 packaging/linux/sudokura.desktop AppDir/usr/share/applications/sudokura.desktop
install -Dm644 assets/generated/sudokura-256.png AppDir/usr/share/icons/hicolor/256x256/apps/sudokura.png
install -Dm644 "$font" AppDir/usr/bin/DejaVuSans.ttf
install -Dm644 packaging/licenses/DejaVu-FONT-LICENSE.txt AppDir/usr/bin/DejaVu-FONT-LICENSE.txt
install -Dm644 assets/audio/music-main.ogg AppDir/usr/bin/audio/music-main.ogg
install -Dm644 assets/audio/music-fail.ogg AppDir/usr/bin/audio/music-fail.ogg
install -Dm644 assets/audio/jingle-win.ogg AppDir/usr/bin/audio/jingle-win.ogg
install -Dm644 assets/audio/jingle-fail.ogg AppDir/usr/bin/audio/jingle-fail.ogg
for audio in music-main.ogg music-fail.ogg jingle-win.ogg jingle-fail.ogg; do test -s "AppDir/usr/bin/audio/$audio"; done
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 30s AppDir/usr/bin/sudokura --smoke-test
"$LINUXDEPLOY" --appdir AppDir --output appimage
mv Sudokura*.AppImage "Sudokura-v${VERSION}-linux-x86_64.AppImage"; chmod +x "Sudokura-v${VERSION}-linux-x86_64.AppImage"
test -x "Sudokura-v${VERSION}-linux-x86_64.AppImage"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy APPIMAGE_EXTRACT_AND_RUN=1 timeout 30s "./Sudokura-v${VERSION}-linux-x86_64.AppImage" --smoke-test
find AppDir -type f -printf '%P\n' | sort > inventory-linux.txt
for audio in music-main.ogg music-fail.ogg jingle-win.ogg jingle-fail.ogg; do grep -Fxq "usr/bin/audio/$audio" inventory-linux.txt; done
ldd AppDir/usr/bin/sudokura | tee dependencies-linux.txt
grep -qi 'SDL2_mixer' dependencies-linux.txt
sha256sum "Sudokura-v${VERSION}-linux-x86_64.AppImage" > SHA256SUMS-linux.txt
du -h "Sudokura-v${VERSION}-linux-x86_64.AppImage"
