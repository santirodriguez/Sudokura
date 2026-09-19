#!/usr/bin/env bash
set -euo pipefail

artifact=${1:?AppImage path required}
label=${2:-linux}
fuse_mode=${3:-skip}
artifact=$(realpath "$artifact")
test -x "$artifact"

root=$(mktemp -d)
cleanup() {
  if [[ -n "${weston_pid:-}" ]]; then kill "$weston_pid" 2>/dev/null || true; fi
  if [[ -n "${xvfb_pid:-}" ]]; then kill "$xvfb_pid" 2>/dev/null || true; fi
  rm -rf "$root"
}
trap cleanup EXIT

candidate="$root/Sudokura prueba á漢.AppImage"
cp "$artifact" "$candidate"
chmod +x "$candidate"

mkdir -p "$root/home" "$root/xdg-data" "$root/xdg-config" "$root/xdg-cache" "$root/runtime" "$root/cwd"
chmod 700 "$root/runtime"
report="linux-runtime-${label}.txt"
: > "$report"

{
  printf 'label=%s\n' "$label"
  printf 'artifact=%s\n' "$(basename "$artifact")"
  uname -a
  cat /etc/os-release
} >> "$report"

common_env=(
  "HOME=$root/home"
  "XDG_DATA_HOME=$root/xdg-data"
  "XDG_CONFIG_HOME=$root/xdg-config"
  "XDG_CACHE_HOME=$root/xdg-cache"
  "XDG_RUNTIME_DIR=$root/runtime"
  "SDL_AUDIODRIVER=dummy"
  "SDL_RENDER_DRIVER=software"
  "SDL_RENDER_VSYNC=0"
)

(
  cd "$root/cwd"
  env "${common_env[@]}" SDL_VIDEODRIVER=dummy     timeout 45s "$candidate" --appimage-extract-and-run --smoke-test
)
printf 'extract_and_run=PASS\n' >> "$report"

if find "$root/cwd" -mindepth 1 -print -quit | grep -q .; then
  echo 'application wrote into foreign working directory' >&2
  find "$root/cwd" -mindepth 1 -maxdepth 2 -print >&2
  exit 1
fi
printf 'foreign_cwd_clean=PASS\nxdg_environment=PASS\n' >> "$report"

if [[ "$fuse_mode" == required ]]; then
  test -e /dev/fuse
  (
    cd "$root/cwd"
    env "${common_env[@]}" SDL_VIDEODRIVER=dummy       timeout 45s "$candidate" --smoke-test
  )
  printf 'fuse_mount=PASS\n' >> "$report"
else
  printf 'fuse_mount=SKIPPED_BY_ENVIRONMENT\n' >> "$report"
fi

display=:93
Xvfb "$display" -screen 0 1280x800x24 -nolisten tcp > "$root/xvfb.log" 2>&1 &
xvfb_pid=$!
for _ in $(seq 1 50); do
  kill -0 "$xvfb_pid" 2>/dev/null || { cat "$root/xvfb.log" >&2; exit 1; }
  [[ -S "/tmp/.X11-unix/X${display#:}" ]] && break
  sleep 0.1
done
(
  cd "$root/cwd"
  env "${common_env[@]}" DISPLAY="$display" SDL_VIDEODRIVER=x11     timeout 45s "$candidate" --appimage-extract-and-run --smoke-test
)
printf 'sdl_x11_native_probe=PASS\n' >> "$report"
kill "$xvfb_pid" 2>/dev/null || true
wait "$xvfb_pid" 2>/dev/null || true
unset xvfb_pid

socket=sudokura-wayland
weston_started=0
for backend in headless-backend.so headless; do
  rm -f "$root/runtime/$socket"
  env XDG_RUNTIME_DIR="$root/runtime"     weston --backend="$backend" --socket="$socket" --idle-time=0 --log="$root/weston.log" > /dev/null 2>&1 &
  weston_pid=$!
  for _ in $(seq 1 60); do
    if [[ -S "$root/runtime/$socket" ]]; then weston_started=1; break; fi
    if ! kill -0 "$weston_pid" 2>/dev/null; then break; fi
    sleep 0.1
  done
  [[ "$weston_started" == 1 ]] && break
  kill "$weston_pid" 2>/dev/null || true
  wait "$weston_pid" 2>/dev/null || true
  unset weston_pid
done
if [[ "$weston_started" != 1 ]]; then
  cat "$root/weston.log" >&2 || true
  echo 'failed to start headless Wayland compositor' >&2
  exit 1
fi
(
  cd "$root/cwd"
  env "${common_env[@]}" WAYLAND_DISPLAY="$socket" SDL_VIDEODRIVER=wayland     timeout 45s "$candidate" --appimage-extract-and-run --smoke-test
)
printf 'sdl_wayland_native_probe=PASS\n' >> "$report"
kill "$weston_pid" 2>/dev/null || true
wait "$weston_pid" 2>/dev/null || true
unset weston_pid

cat "$report"
