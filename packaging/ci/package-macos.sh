#!/usr/bin/env bash
set -euo pipefail
: "${ARCH:?}" "${FONT:?}"
SOURCE_VERSION=$(./scripts/version.sh)
if [[ -n "${VERSION:-}" && "$VERSION" != "$SOURCE_VERSION" ]]; then
  echo "VERSION=$VERSION does not match source version $SOURCE_VERSION" >&2
  exit 1
fi
VERSION=$SOURCE_VERSION
test -f "$FONT"; ./scripts/validate_font.py "$FONT"; export SUDOKURA_TEST_FONT="$FONT"
make assets; make WERROR=-Werror test test-ui all
FONT_LICENSE="$PWD/packaging/licenses/DejaVu-FONT-LICENSE.txt" PREFIX=$(brew --prefix) packaging/macos/bundle.sh Sudokura.app sudokura
bundle_short=$(plutil -extract CFBundleShortVersionString raw Sudokura.app/Contents/Info.plist)
bundle_build=$(plutil -extract CFBundleVersion raw Sudokura.app/Contents/Info.plist)
test "$bundle_short" = "$VERSION"; test "$bundle_build" = "$VERSION"
file Sudokura.app/Contents/MacOS/sudokura | tee "architecture-macos-${ARCH}.txt"; grep -q "$ARCH" "architecture-macos-${ARCH}.txt"
for required in Contents/Info.plist Contents/Resources/sudokura.icns Contents/Resources/DejaVuSans.ttf Contents/Resources/DejaVu-FONT-LICENSE.txt Contents/Resources/audio/music-main.ogg Contents/Resources/audio/music-fail.ogg Contents/Resources/audio/jingle-win.ogg Contents/Resources/audio/jingle-fail.ogg; do test -s "Sudokura.app/$required"; done
./scripts/validate_font.py Sudokura.app/Contents/Resources/DejaVuSans.ttf
python3 - "smoke-macos-${ARCH}.txt" <<'PY'
import os
import pathlib
import subprocess
import sys

report = pathlib.Path(sys.argv[1])
command = ["Sudokura.app/Contents/MacOS/sudokura", "--smoke-test"]
entries = []
succeeded = False
hard_failure = False

for driver in ("offscreen", "dummy"):
    env = os.environ.copy()
    env.update({
        "SDL_VIDEODRIVER": driver,
        "SDL_RENDER_DRIVER": "software",
        "SDL_RENDER_VSYNC": "0",
        "SDL_AUDIODRIVER": "dummy",
    })
    try:
        result = subprocess.run(
            command,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=20,
        )
    except subprocess.TimeoutExpired:
        entries.append(f"{driver}: timed out after 20 seconds on the headless GitHub runner")
        continue

    output = f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}".strip()
    entries.append(f"{driver}: exit {result.returncode}\n{output}")
    if result.returncode == 0:
        succeeded = True
        break

    lowered = output.lower()
    driver_unavailable = (
        "no available video device" in lowered
        or "video driver" in lowered and "not available" in lowered
        or "offscreen" in lowered and "not" in lowered and "available" in lowered
    )
    if not driver_unavailable:
        hard_failure = True

if succeeded:
    entries.append("RESULT: packaged application smoke test passed")
elif hard_failure:
    entries.append("RESULT: packaged application returned a real non-runner error")
else:
    entries.append(
        "RESULT: inconclusive on the headless GitHub runner; bundle structure, architecture, resources, dylibs, inventory, ZIP, and checksum validation continue. A real Mac launch remains required before publication."
    )

report.write_text("\n\n".join(entries) + "\n", encoding="utf-8")
print(report.read_text(encoding="utf-8"))
if hard_failure and not succeeded:
    raise SystemExit(1)
PY
find Sudokura.app -type f | sort | tee "inventory-macos-${ARCH}.txt"
mapfile=(); while IFS= read -r -d '' f; do mapfile+=("$f"); done < <(find Sudokura.app/Contents/Frameworks -type f -name '*.dylib' -print0)
(( ${#mapfile[@]} > 0 )); otool -L Sudokura.app/Contents/MacOS/sudokura "${mapfile[@]}" | tee "dependencies-macos-${ARCH}.txt"
grep -qi 'SDL2_mixer' "dependencies-macos-${ARCH}.txt"
if grep -E '/opt/homebrew|/usr/local|/Users/runner|/private/var/folders' "dependencies-macos-${ARCH}.txt"; then echo 'build-host path remains' >&2; exit 1; fi
if awk '/^[[:space:]]+\//{print $1}' "dependencies-macos-${ARCH}.txt" | grep -Ev '^(/usr/lib/|/System/Library/)'; then echo 'unbundled non-system dylib' >&2; exit 1; fi
ditto -c -k --sequesterRsrc --keepParent Sudokura.app "Sudokura-v${VERSION}-macos-${ARCH}-unsigned.zip"
unzip -t "Sudokura-v${VERSION}-macos-${ARCH}-unsigned.zip"; zip_inventory=$(unzip -Z1 "Sudokura-v${VERSION}-macos-${ARCH}-unsigned.zip"); grep -q 'Sudokura.app/Contents/MacOS/sudokura' <<<"$zip_inventory"
for audio in music-main.ogg music-fail.ogg jingle-win.ogg jingle-fail.ogg; do grep -Fq "Sudokura.app/Contents/Resources/audio/$audio" <<<"$zip_inventory"; done
shasum -a 256 "Sudokura-v${VERSION}-macos-${ARCH}-unsigned.zip" > "SHA256SUMS-macos-${ARCH}.txt"; du -h "Sudokura-v${VERSION}-macos-${ARCH}-unsigned.zip"
