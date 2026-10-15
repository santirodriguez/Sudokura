#!/usr/bin/env bash
set -euo pipefail

# Official linuxdeploy x86_64 asset from the stable 1-alpha-20251107-1 release.
# The GitHub release-asset ID identifies that uploaded object; the SHA-256
# digest below is GitHub's published digest for the exact asset.
# Download the public release file directly; the REST asset endpoint can reject
# unauthenticated binary requests. Verify exact asset bytes before execution.
readonly RELEASE_TAG=1-alpha-20251107-1
readonly ASSET_ID=313839329
readonly SHA256=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
readonly URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/${RELEASE_TAG}/linuxdeploy-x86_64.AppImage"
output=${1:-linuxdeploy}

curl --fail --location --retry 3 --retry-delay 2 \
  -H 'Accept: application/octet-stream' \
  -H 'X-GitHub-Api-Version: 2022-11-28' \
  "$URL" -o "$output"
printf '%s  %s\n' "$SHA256" "$output" | sha256sum -c -
chmod +x "$output"
printf 'linuxdeploy_release=%s\nlinuxdeploy_asset_id=%s\nlinuxdeploy_sha256=%s\n' \
  "$RELEASE_TAG" "$ASSET_ID" "$SHA256"
