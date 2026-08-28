#!/usr/bin/env bash
set -euo pipefail

# Official linuxdeploy x86_64 release asset published 2026-08-01.
# The asset ID is immutable; the SHA-256 digest is the value published by
# GitHub for that exact upstream asset.
readonly ASSET_ID=497463883
readonly SHA256=421ca71d5c69ea97c6309276232990d43df1dcece0edfaa26bbf926ff96ed12e
readonly URL="https://api.github.com/repos/linuxdeploy/linuxdeploy/releases/assets/${ASSET_ID}"
output=${1:-linuxdeploy}

curl --fail --location --retry 3 --retry-delay 2 \
  -H 'Accept: application/octet-stream' \
  -H 'X-GitHub-Api-Version: 2022-11-28' \
  "$URL" -o "$output"
printf '%s  %s\n' "$SHA256" "$output" | sha256sum -c -
chmod +x "$output"
