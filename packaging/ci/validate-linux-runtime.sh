#!/usr/bin/env bash
set -euo pipefail

artifact=${1:?AppImage path required}
label=${2:-linux}
fuse_mode=${3:-skip}
artifact=$(realpath "$artifact")
test -x "$artifact"

root=$(mktemp -d)
report="linux-runtime-${label}.txt"
: > "$report"
current_stage=setup
cleanup() {
  status=$?
  if [[ "$status" == 0 ]]; then
    printf 'result=PASS\n' >> "$report"
  else
    printf 'result=FAIL\nfailed_stage=%s\nexit_code=%s\n' \
      "$current_stage" "$status" >> "$report"
  fi
  if [[ -n "${weston_pid:-}" ]]; then kill "$weston_pid" 2>/dev/null || true; fi
  if [[ -n "${xvfb_pid:-}" ]]; then kill "$xvfb_pid" 2>/dev/null || true; fi
  rm -rf "$root"
  cat "$report"
}
trap cleanup EXIT

run_stage() {
  current_stage=$1
  printf 'stage_%s=RUNNING\n' "$current_stage" >> "$report"
}

pass_stage() {
  printf 'stage_%s=PASS\n' "$current_stage" >> "$report"
}

candidate="$root/Sudokura prueba á漢.AppImage"
cp "$artifact" "$candidate"
chmod +x "$candidate"

mkdir -p "$root/home" "$root/xdg-data" "$root/xdg-config" "$root/xdg-cache" "$root/runtime" "$root/cwd"
chmod 700 "$root/runtime"

{
  printf 'label=%s\n' "$label"
  printf 'artifact=%s\n' "$(basename "$artifact")"
  uname -a
  cat /etc/os-release
} >> "$report"

common_env=(
  "PATH=/usr/bin:/bin"
  "LANG=C.UTF-8"
  "LC_ALL=C.UTF-8"
  "HOME=$root/home"
  "XDG_DATA_HOME=$root/xdg-data"
  "XDG_CONFIG_HOME=$root/xdg-config"
  "XDG_CACHE_HOME=$root/xdg-cache"
  "XDG_RUNTIME_DIR=$root/runtime"
  "SDL_AUDIODRIVER=dummy"
  "SDL_RENDER_DRIVER=software"
  "SDL_RENDER_VSYNC=0"
)

run_stage extract_payload
mkdir -p "$root/extracted"
(
  cd "$root/extracted"
  env -i "${common_env[@]}" "$candidate" --appimage-extract >/dev/null
)
extracted="$root/extracted/squashfs-root"
test -x "$extracted/AppRun"
test ! -L "$extracted/AppRun"
test -x "$extracted/usr/bin/sudokura"
test -f "$extracted/usr/lib/libdecor/plugins-1/libdecor-cairo.so"
pass_stage

run_stage recursive_elf_audit
bundle_search="$extracted/usr/lib:$extracted/usr/lib/libdecor/plugins-1:$extracted/usr/bin"
while IFS= read -r -d '' file_path; do
  if readelf -h "$file_path" >/dev/null 2>&1; then
    rel=${file_path#"$extracted/"}
    printf '\n[elf:%s]\n' "$rel" >> "$report"
    readelf -l "$file_path" 2>/dev/null \
      | sed -n 's/.*Requesting program interpreter: \(.*\)]/interpreter=\1/p' \
      >> "$report"
    readelf -d "$file_path" 2>/dev/null \
      | sed -n \
          -e 's/.*(NEEDED).*\[\(.*\)\]/needed=\1/p' \
          -e 's/.*(RPATH).*\[\(.*\)\]/rpath=\1/p' \
          -e 's/.*(RUNPATH).*\[\(.*\)\]/runpath=\1/p' \
      >> "$report"
    resolution=$(LD_LIBRARY_PATH="$bundle_search" ldd "$file_path")
    printf '%s\n' "$resolution" >> "$report"
    if grep -q 'not found' <<<"$resolution"; then
      echo "unresolved off-build-host ELF dependency in $rel" >&2
      exit 1
    fi
  fi
done < <(find "$extracted" -type f -print0)
pass_stage

run_stage direct_binary
(
  cd "$root/cwd"
  env -i "${common_env[@]}" \
    LD_LIBRARY_PATH="$bundle_search" \
    LIBDECOR_PLUGIN_DIR="$extracted/usr/lib/libdecor/plugins-1" \
    SDL_VIDEODRIVER=dummy \
    timeout 45s "$extracted/usr/bin/sudokura" --smoke-test
)
pass_stage

run_stage extracted_apprun
(
  cd "$root/cwd"
  env -i "${common_env[@]}" SDL_VIDEODRIVER=dummy \
    timeout 45s "$extracted/AppRun" --smoke-test
)
pass_stage

run_stage extract_and_run
(
  cd "$root/cwd"
  env -i "${common_env[@]}" SDL_VIDEODRIVER=dummy \
    timeout 45s "$candidate" --appimage-extract-and-run --smoke-test
)
pass_stage

if find "$root/cwd" -mindepth 1 -print -quit | grep -q .; then
  echo 'application wrote into foreign working directory' >&2
  find "$root/cwd" -mindepth 1 -maxdepth 2 -print >&2
  exit 1
fi
printf 'foreign_cwd_clean=PASS\nxdg_environment=PASS\n' >> "$report"

if [[ "$fuse_mode" == required ]]; then
  run_stage fuse_mount
  test -e /dev/fuse
  (
    cd "$root/cwd"
    env -i "${common_env[@]}" SDL_VIDEODRIVER=dummy \
      timeout 45s "$candidate" --smoke-test
  )
  pass_stage
else
  printf 'stage_fuse_mount=SKIPPED_BY_ENVIRONMENT\n' >> "$report"
fi

run_stage x11
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
  env -i "${common_env[@]}" DISPLAY="$display" SDL_VIDEODRIVER=x11 \
    timeout 45s "$candidate" --appimage-extract-and-run --smoke-test
)
pass_stage
kill "$xvfb_pid" 2>/dev/null || true
wait "$xvfb_pid" 2>/dev/null || true
unset xvfb_pid

socket=sudokura-wayland
weston_started=0
run_stage wayland_compositor
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
pass_stage
run_stage wayland
(
  cd "$root/cwd"
  env -i "${common_env[@]}" WAYLAND_DISPLAY="$socket" SDL_VIDEODRIVER=wayland \
    timeout 45s "$candidate" --appimage-extract-and-run --smoke-test
)
pass_stage
kill "$weston_pid" 2>/dev/null || true
wait "$weston_pid" 2>/dev/null || true
unset weston_pid

if find "$root/cwd" -mindepth 1 -print -quit | grep -q .; then
  echo 'application wrote into foreign working directory after display probes' >&2
  find "$root/cwd" -mindepth 1 -maxdepth 2 -print >&2
  exit 1
fi
printf 'foreign_cwd_final_clean=PASS\n' >> "$report"
