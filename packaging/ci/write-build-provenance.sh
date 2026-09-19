#!/usr/bin/env bash
set -euo pipefail
platform=${1:?platform label required}
output=${2:?output path required}
{
  printf 'product=Sudokura\n'
  printf 'version=%s\n' "$(./scripts/version.sh)"
  printf 'source_commit=%s\n' "$(git rev-parse HEAD)"
  printf 'artifact_kind=%s\n' "${SUDOKURA_ARTIFACT_KIND:-candidate}"
  printf 'platform=%s\n' "$platform"
  printf 'github_run_id=%s\n' "${GITHUB_RUN_ID:-local}"
  printf 'github_run_attempt=%s\n' "${GITHUB_RUN_ATTEMPT:-local}"
  printf '\n[os]\n'
  uname -a || true
  printf '\n[compiler]\n'
  (cc --version || gcc --version || clang --version) 2>&1 | head -5 || true
  printf '\n[go]\n'
  if [[ -n "${SUDOKURA_GO:-}" && -x "${SUDOKURA_GO:-}" ]]; then "$SUDOKURA_GO" version; elif command -v go >/dev/null 2>&1; then go version; else echo unavailable; fi
  printf '\n[python]\n'
  python3 --version 2>&1 || true
  printf '\n[pkg-config]\n'
  pkg-config --version 2>&1 || true
  for module in sdl2 SDL2_ttf SDL2_mixer; do
    printf '%s=' "$module"
    pkg-config --modversion "$module" 2>/dev/null || echo unavailable
  done
  if command -v pacman >/dev/null 2>&1; then
    printf '\n[msys2-packages]\n'
    pacman -Q mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-SDL2_mixer mingw-w64-x86_64-ttf-dejavu 2>/dev/null || true
  elif command -v dpkg-query >/dev/null 2>&1; then
    printf '\n[debian-packages]\n'
    dpkg-query -W -f='${Package} ${Version}\n' libsdl2-2.0-0 libsdl2-ttf-2.0-0 libsdl2-mixer-2.0-0 fonts-dejavu-core 2>/dev/null || true
  elif command -v brew >/dev/null 2>&1; then
    printf '\n[homebrew-packages]\n'
    brew list --versions sdl2 sdl2_ttf sdl2_mixer pkg-config 2>/dev/null || true
  fi
} | tee "$output"
