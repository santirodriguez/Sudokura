#!/usr/bin/env bash
set -euo pipefail
: "${LINUXDEPLOY:?path to linuxdeploy}"
: "${APPIMAGETOOL:?path to appimagetool}"
: "${APPIMAGE_RUNTIME:?path to pinned AppImage runtime}"

os_id=$(sh -c '. /etc/os-release; printf "%s" "${ID:-}"')
os_version_id=$(sh -c '. /etc/os-release; printf "%s" "${VERSION_ID:-}"')
os_pretty=$(sh -c '. /etc/os-release; printf "%s" "${PRETTY_NAME:-unknown}"')
if [[ "$os_id" != ubuntu || "$os_version_id" != 22.04 ]]; then
  echo "Linux candidate must be built on Ubuntu 22.04; got $os_pretty" >&2
  exit 1
fi

SOURCE_VERSION=$(./scripts/version.sh)
if [[ -n "${VERSION:-}" && "$VERSION" != "$SOURCE_VERSION" ]]; then
  echo "VERSION=$VERSION does not match source version $SOURCE_VERSION" >&2
  exit 1
fi
VERSION=$SOURCE_VERSION
SOURCE_COMMIT=$(./scripts/source_commit.sh)
export SUDOKURA_SOURCE_COMMIT="$SOURCE_COMMIT"

font=$(fc-match -f '%{file}' 'DejaVu Sans')
test -f "$font"
./scripts/validate_font.py "$font"

rm -rf AppDir
install -Dm755 sudokura AppDir/usr/bin/sudokura
install -Dm644 packaging/linux/sudokura.desktop AppDir/usr/share/applications/sudokura.desktop
install -Dm644 assets/generated/sudokura-256.png AppDir/usr/share/icons/hicolor/256x256/apps/sudokura.png
install -Dm644 "$font" AppDir/usr/bin/DejaVuSans.ttf
install -Dm644 packaging/licenses/DejaVu-FONT-LICENSE.txt AppDir/usr/bin/DejaVu-FONT-LICENSE.txt
install -Dm644 LICENSE AppDir/usr/share/doc/sudokura/LICENSE.txt
install -Dm644 packaging/licenses/DISTRIBUTION-NOTICES.md AppDir/usr/share/doc/sudokura/DISTRIBUTION-NOTICES.md
install -Dm644 assets/audio/README.md AppDir/usr/share/doc/sudokura/AUDIO-PROVENANCE.md
install -Dm644 assets/audio/music-main.ogg AppDir/usr/bin/audio/music-main.ogg
install -Dm644 assets/audio/music-fail.ogg AppDir/usr/bin/audio/music-fail.ogg
install -Dm644 assets/audio/jingle-win.ogg AppDir/usr/bin/audio/jingle-win.ogg
install -Dm644 assets/audio/jingle-fail.ogg AppDir/usr/bin/audio/jingle-fail.ogg
for audio in music-main.ogg music-fail.ogg jingle-win.ogg jingle-fail.ogg; do
  test -s "AppDir/usr/bin/audio/$audio"
done

SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 30s AppDir/usr/bin/sudokura --smoke-test
"$LINUXDEPLOY" --appdir AppDir

desktop-file-validate AppDir/sudokura.desktop
grep -Fxq 'Type=Application' AppDir/sudokura.desktop
grep -Fxq 'Exec=sudokura' AppDir/sudokura.desktop
grep -Fxq 'Icon=sudokura' AppDir/sudokura.desktop
grep -Fxq 'Terminal=false' AppDir/sudokura.desktop
test -x AppDir/AppRun
test -s AppDir/sudokura.png

./packaging/ci/audit-linux-bundle.sh AppDir

artifact="Sudokura-v${VERSION}-linux-x86_64.AppImage"
VERSION="$VERSION" ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$APPIMAGETOOL" \
  --runtime-file "$APPIMAGE_RUNTIME" AppDir "$artifact"
grep -Fxq "X-AppImage-Version=$VERSION" AppDir/sudokura.desktop
chmod +x "$artifact"
test -x "$artifact"

SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_RENDER_DRIVER=software SDL_RENDER_VSYNC=0 \
  timeout 45s "./$artifact" --appimage-extract-and-run --smoke-test

find AppDir -type f -printf '%P\n' | LC_ALL=C sort > inventory-linux.txt
for audio in music-main.ogg music-fail.ogg jingle-win.ogg jingle-fail.ogg; do
  grep -Fxq "usr/bin/audio/$audio" inventory-linux.txt
done
grep -Fxq 'usr/share/doc/sudokura/COMPONENTS.txt' inventory-linux.txt
grep -qi 'SDL2_mixer' dependencies-linux-all.txt

sha256sum "$artifact" > SHA256SUMS-linux.txt
./packaging/ci/write-build-provenance.sh linux build-provenance-linux.txt
python3 scripts/write_artifact_manifest.py \
  --version "$VERSION" --source-commit "$SOURCE_COMMIT" \
  --platform linux --architecture x86_64 \
  --minimum 'Ubuntu 22.04 x86_64 build baseline; Ubuntu 24.04/Fedora 44 automated portability probes; real desktop acceptance remains Phase 8' \
  --kind "${SUDOKURA_ARTIFACT_KIND:-candidate}" \
  --output artifact-manifest-linux.json \
  --artifact "$artifact" --baseline 9730552
du -h "$artifact"
