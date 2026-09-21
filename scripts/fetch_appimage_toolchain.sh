#!/usr/bin/env bash
set -euo pipefail

./scripts/fetch_linuxdeploy.sh "${1:-linuxdeploy}"

# Stable appimagetool 1.9.1 x86_64 release asset.
readonly APPIMAGETOOL_RELEASE=1.9.1
readonly APPIMAGETOOL_ASSET_ID=324406736
readonly APPIMAGETOOL_SHA256=ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0
readonly APPIMAGETOOL_URL="https://api.github.com/repos/AppImage/appimagetool/releases/assets/${APPIMAGETOOL_ASSET_ID}"
readonly APPIMAGETOOL_OUTPUT=${2:-appimagetool}

# Stable type2 runtime release used explicitly by appimagetool. This prevents
# appimagetool from fetching the mutable 'continuous' runtime during packaging.
readonly RUNTIME_RELEASE=20251108
readonly RUNTIME_ASSET_ID=326011592
readonly RUNTIME_SHA256=2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d
readonly RUNTIME_URL="https://api.github.com/repos/AppImage/type2-runtime/releases/assets/${RUNTIME_ASSET_ID}"
readonly RUNTIME_OUTPUT=${3:-appimage-runtime}

fetch_asset() {
  local url=$1 output=$2 sha=$3
  curl --fail --location --retry 3 --retry-delay 2 \
    -H 'Accept: application/octet-stream' \
    -H 'X-GitHub-Api-Version: 2022-11-28' \
    "$url" -o "$output"
  printf '%s  %s\n' "$sha" "$output" | sha256sum -c -
}

fetch_asset "$APPIMAGETOOL_URL" "$APPIMAGETOOL_OUTPUT" "$APPIMAGETOOL_SHA256"
chmod +x "$APPIMAGETOOL_OUTPUT"
fetch_asset "$RUNTIME_URL" "$RUNTIME_OUTPUT" "$RUNTIME_SHA256"

printf 'appimagetool_release=%s\nappimagetool_asset_id=%s\nappimagetool_sha256=%s\n' \
  "$APPIMAGETOOL_RELEASE" "$APPIMAGETOOL_ASSET_ID" "$APPIMAGETOOL_SHA256"
printf 'appimage_runtime_release=%s\nappimage_runtime_asset_id=%s\nappimage_runtime_sha256=%s\n' \
  "$RUNTIME_RELEASE" "$RUNTIME_ASSET_ID" "$RUNTIME_SHA256"
