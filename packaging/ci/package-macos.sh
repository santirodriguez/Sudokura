#!/usr/bin/env bash
set -euo pipefail
: "${ARCH:?}" "${FONT:?}"

MIN_MACOS=15.0
if [[ "$ARCH" != arm64 ]]; then
  echo "v1.3 macOS package is arm64-only" >&2
  exit 1
fi
if [[ "$(uname -m)" != arm64 ]]; then
  echo "macOS v1.3 package must be built on an Apple Silicon runner" >&2
  exit 1
fi
python3 - "$(sw_vers -productVersion)" "$MIN_MACOS" <<'PY'
import sys
parts=lambda value: tuple(int(x) for x in value.split(".")[:2])
if parts(sys.argv[1]) < parts(sys.argv[2]):
    raise SystemExit(f"macOS runner {sys.argv[1]} is below required {sys.argv[2]}")
PY

SOURCE_VERSION=$(./scripts/version.sh)
if [[ -n "${VERSION:-}" && "$VERSION" != "$SOURCE_VERSION" ]]; then
  echo "VERSION=$VERSION does not match source version $SOURCE_VERSION" >&2
  exit 1
fi
VERSION=$SOURCE_VERSION
SOURCE_COMMIT=$(./scripts/source_commit.sh)
export SUDOKURA_SOURCE_COMMIT="$SOURCE_COMMIT"
export MACOSX_DEPLOYMENT_TARGET="$MIN_MACOS"

test -f "$FONT"
./scripts/validate_font.py "$FONT"
export SUDOKURA_TEST_FONT="$FONT"

make assets
make clean
MACOSX_DEPLOYMENT_TARGET="$MIN_MACOS" make WERROR=-Werror test test-ui all

FONT_LICENSE="$PWD/packaging/licenses/DejaVu-FONT-LICENSE.txt" \
PREFIX=$(brew --prefix) ARCH="$ARCH" MIN_MACOS="$MIN_MACOS" \
  packaging/macos/bundle.sh Sudokura.app sudokura

bundle_short=$(plutil -extract CFBundleShortVersionString raw Sudokura.app/Contents/Info.plist)
bundle_build=$(plutil -extract CFBundleVersion raw Sudokura.app/Contents/Info.plist)
bundle_min=$(plutil -extract LSMinimumSystemVersion raw Sudokura.app/Contents/Info.plist)
test "$bundle_short" = "$VERSION"
test "$bundle_build" = "$VERSION"
test "$bundle_min" = "$MIN_MACOS"

for required in \
  Contents/Info.plist \
  Contents/Resources/sudokura.icns \
  Contents/Resources/DejaVuSans.ttf \
  Contents/Resources/DejaVu-FONT-LICENSE.txt \
  Contents/Resources/Documentation/LICENSE.txt \
  Contents/Resources/Documentation/DISTRIBUTION-NOTICES.md \
  Contents/Resources/Documentation/COMPONENTS.txt \
  Contents/Resources/audio/music-main.ogg \
  Contents/Resources/audio/music-fail.ogg \
  Contents/Resources/audio/jingle-win.ogg \
  Contents/Resources/audio/jingle-fail.ogg
do
  test -s "Sudokura.app/$required"
done
./scripts/validate_font.py Sudokura.app/Contents/Resources/DejaVuSans.ttf

python3 packaging/ci/audit-macos-bundle.py Sudokura.app "$VERSION" "$MIN_MACOS" \
  | tee "mach-o-audit-macos-${ARCH}.json"
python3 packaging/ci/smoke-macos-bundle.py \
  Sudokura.app/Contents/MacOS/sudokura "smoke-macos-${ARCH}-bundle.txt" bundle

find Sudokura.app -type f | LC_ALL=C sort | tee "inventory-macos-${ARCH}.txt"
machos=()
while IFS= read -r file_path; do
  if file -b "$file_path" | grep -q 'Mach-O'; then machos+=("$file_path"); fi
done < <(find Sudokura.app -type f -print)
(( ${#machos[@]} > 0 ))
file "${machos[@]}" | tee "architecture-macos-${ARCH}.txt"
otool -L "${machos[@]}" | tee "dependencies-macos-${ARCH}.txt"
grep -qi 'SDL2_mixer' "dependencies-macos-${ARCH}.txt"
codesign -dv --verbose=4 Sudokura.app 2>&1 | tee "codesign-macos-${ARCH}.txt"
grep -q 'Signature=adhoc' "codesign-macos-${ARCH}.txt"

artifact="Sudokura-v${VERSION}-macos-${ARCH}.dmg"
rm -rf dmgroot macmount attach.plist
mkdir dmgroot
ditto Sudokura.app dmgroot/Sudokura.app
ln -s /Applications dmgroot/Applications
hdiutil create -quiet -ov -format UDZO -volname Sudokura -srcfolder dmgroot "$artifact"
hdiutil verify "$artifact" | tee "dmg-verify-macos-${ARCH}.txt"

mkdir macmount
hdiutil attach -readonly -nobrowse -mountpoint "$PWD/macmount" -plist "$artifact" > attach.plist
trap 'hdiutil detach "$PWD/macmount" >/dev/null 2>&1 || true' EXIT
test -d macmount/Sudokura.app
test -L macmount/Applications
codesign --verify --deep --strict --verbose=2 macmount/Sudokura.app
python3 packaging/ci/smoke-macos-bundle.py \
  macmount/Sudokura.app/Contents/MacOS/sudokura "smoke-macos-${ARCH}-dmg.txt" dmg
hdiutil detach "$PWD/macmount"
trap - EXIT
rm -rf macmount attach.plist dmgroot

shasum -a 256 "$artifact" > "SHA256SUMS-macos-${ARCH}.txt"
python3 - "$artifact" > "size-comparison-macos-${ARCH}.txt" <<'PY'
import os
import sys
old = 6306753
new = os.path.getsize(sys.argv[1])
delta = new - old
pct = delta * 100.0 / old
print("v1.2_reference=Sudokura-v1.2.0-macos-arm64-unsigned.zip")
print(f"v1.2_reference_bytes={old}")
print(f"v1.3_dmg_bytes={new}")
print(f"delta_bytes={delta}")
print(f"delta_percent={pct:.3f}")
print("comparison_note=Format changed from ZIP to DMG; delta is contextual, not like-for-like.")
PY

./packaging/ci/write-build-provenance.sh "macos-${ARCH}" "build-provenance-macos-${ARCH}.txt"
python3 scripts/write_artifact_manifest.py \
  --version "$VERSION" --source-commit "$SOURCE_COMMIT" \
  --platform macos --architecture "$ARCH" \
  --minimum 'macOS 15.0+ arm64; ad-hoc integrity signature only; experimental pending real-device/Gatekeeper acceptance' \
  --kind "${SUDOKURA_ARTIFACT_KIND:-candidate}" \
  --output "artifact-manifest-macos-${ARCH}.json" \
  --artifact "$artifact" --baseline 0

du -h "$artifact"
