#!/usr/bin/env bash
set -euo pipefail

artifact=${1:?AppImage path required}
label=${2:-linux}
fuse_mode=${3:-skip}
artifact=$(realpath "$artifact")
test -x "$artifact"

root=$(mktemp -d)
report="$PWD/linux-runtime-${label}.txt"
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
  if [[ -n "${dbus_pid:-}" ]]; then kill "$dbus_pid" 2>/dev/null || true; fi
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

append_log() {
  local name=$1 path=$2
  printf '\n[%s]\n' "$name" >> "$report"
  if [[ -s "$path" ]]; then
    cat "$path" >> "$report"
  else
    printf '(no output)\n' >> "$report"
  fi
}

run_captured() {
  local name=$1 output=$2
  shift 2
  if "$@" > "$output" 2>&1; then
    captured_status=0
  else
    captured_status=$?
  fi
  printf 'command_%s_exit=%s\n' "$name" "$captured_status" >> "$report"
  append_log "$name" "$output"
}

candidate="$root/Sudokura prueba á漢.AppImage"
cp "$artifact" "$candidate"
chmod +x "$candidate"

mkdir -p "$root/home" "$root/xdg-data" "$root/xdg-config" "$root/xdg-cache" "$root/runtime" "$root/cwd"
chmod 700 "$root/runtime"

{
  printf 'label=%s\n' "$label"
  printf 'artifact=%s\n' "$(basename "$artifact")"
  for command_name in bash sh realpath timeout seq readelf ldd Xvfb weston \
      dbus-daemon gdb strace; do
    printf 'command_%s=%s\n' "$command_name" "$(command -v "$command_name")"
  done
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
    if resolution=$(LD_LIBRARY_PATH="$bundle_search" ldd -r "$file_path" 2>&1); then
      resolution_status=0
    else
      resolution_status=$?
    fi
    printf '%s\n' "$resolution" >> "$report"
    if [[ "$resolution_status" != 0 ]] ||
       grep -Eq 'not found|undefined symbol' <<<"$resolution"; then
      echo "unresolved off-build-host ELF dependency or relocation in $rel" >&2
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

socket=sudokura-wayland
weston_started=0
weston_backend=
run_stage wayland_session_bus
mapfile -t dbus_metadata < <(
  dbus-daemon --session --fork --print-address=1 --print-pid=1
)
if [[ "${#dbus_metadata[@]}" -lt 2 ]] ||
   [[ -z "${dbus_metadata[0]}" ]] ||
   [[ ! "${dbus_metadata[1]}" =~ ^[0-9]+$ ]]; then
  printf '%s\n' "${dbus_metadata[@]}" >&2
  echo 'failed to start isolated D-Bus session' >&2
  exit 1
fi
dbus_address=${dbus_metadata[0]}
dbus_pid=${dbus_metadata[1]}
kill -0 "$dbus_pid"
printf 'dbus_session_address_scheme=%s\n' "${dbus_address%%:*}" >> "$report"
pass_stage

run_stage wayland_compositor
# Ubuntu 22.04's SDL 2.0.20 predates the fix for input-less Wayland
# compositors. Nest Weston in Xvfb so it advertises a normal wl_seat while
# Sudokura still exercises SDL's native Wayland client and bundled libdecor.
for backend in x11-backend.so x11; do
  rm -f "$root/runtime/$socket"
  rm -f "$root/weston.log" "$root/weston-stdio.log"
  env DISPLAY="$display" \
    XDG_RUNTIME_DIR="$root/runtime" \
    DBUS_SESSION_BUS_ADDRESS="$dbus_address" \
    XDG_SESSION_TYPE=wayland \
    weston --backend="$backend" --renderer=pixman \
      --socket="$socket" --idle-time=0 \
      --log="$root/weston.log" > "$root/weston-stdio.log" 2>&1 &
  weston_pid=$!
  for _ in $(seq 1 60); do
    if [[ -S "$root/runtime/$socket" ]]; then
      weston_started=1
      weston_backend=$backend
      break
    fi
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
  echo 'failed to start nested Wayland compositor' >&2
  exit 1
fi
printf 'weston_backend=%s\nwayland_client_transport=native\n' \
  "$weston_backend" >> "$report"
append_log weston "$root/weston.log"
pass_stage

wayland_env=(
  "${common_env[@]}"
  "DBUS_SESSION_BUS_ADDRESS=$dbus_address"
  "XDG_SESSION_TYPE=wayland"
  "XDG_CURRENT_DESKTOP=Sudokura-CI"
  "WAYLAND_DISPLAY=$socket"
  "SDL_VIDEODRIVER=wayland"
  "SDL_VIDEO_WAYLAND_ALLOW_LIBDECOR=1"
)

diagnose_wayland_failure() {
  local failed_name=$1 failed_status=$2
  printf 'wayland_diagnostic_for=%s\nwayland_diagnostic_original_exit=%s\n' \
    "$failed_name" "$failed_status" >> "$report"

  if command -v gdb >/dev/null 2>&1; then
    run_captured wayland_gdb "$root/wayland-gdb.log" \
      env -i "${wayland_env[@]}" \
        LD_LIBRARY_PATH="$bundle_search" \
        LIBDECOR_PLUGIN_DIR="$extracted/usr/lib/libdecor/plugins-1" \
        WAYLAND_DEBUG=1 \
        timeout 45s gdb --quiet --batch \
          -ex 'set pagination off' \
          -ex run \
          -ex 'thread apply all backtrace full' \
          --args "$extracted/usr/bin/sudokura" --smoke-test
  else
    printf 'wayland_gdb=UNAVAILABLE\n' >> "$report"
  fi

  if command -v strace >/dev/null 2>&1; then
    if env -i "${wayland_env[@]}" \
        LD_LIBRARY_PATH="$bundle_search" \
        LIBDECOR_PLUGIN_DIR="$extracted/usr/lib/libdecor/plugins-1" \
        WAYLAND_DEBUG=1 \
        timeout 45s strace -f -s 256 -o "$root/wayland.strace" \
          "$extracted/usr/bin/sudokura" --smoke-test \
          > "$root/wayland-strace-stdio.log" 2>&1; then
      diagnostic_status=0
    else
      diagnostic_status=$?
    fi
    printf 'command_wayland_strace_exit=%s\n' "$diagnostic_status" >> "$report"
    append_log wayland_strace_stdio "$root/wayland-strace-stdio.log"
    if [[ -f "$root/wayland.strace" ]]; then
      tail -n 500 "$root/wayland.strace" > "$root/wayland-strace-tail.log"
      append_log wayland_strace_tail "$root/wayland-strace-tail.log"
    fi
  else
    printf 'wayland_strace=UNAVAILABLE\n' >> "$report"
  fi

  run_captured wayland_without_libdecor "$root/wayland-without-libdecor.log" \
    env -i "${wayland_env[@]}" \
      SDL_VIDEO_WAYLAND_ALLOW_LIBDECOR=0 \
      LD_LIBRARY_PATH="$bundle_search" \
      LIBDECOR_PLUGIN_DIR="$extracted/usr/lib/libdecor/plugins-1" \
      WAYLAND_DEBUG=1 \
      timeout 45s "$extracted/usr/bin/sudokura" --smoke-test
}

run_stage wayland_extracted_apprun
(
  cd "$root/cwd"
  run_captured wayland_extracted_apprun "$root/wayland-extracted-apprun.log" \
    env -i "${wayland_env[@]}" \
      timeout 45s "$extracted/AppRun" --smoke-test
  if [[ "$captured_status" != 0 ]]; then
    failure_status=$captured_status
    diagnose_wayland_failure wayland_extracted_apprun "$failure_status"
    exit "$failure_status"
  fi
)
pass_stage

run_stage wayland_appimage
(
  cd "$root/cwd"
  run_captured wayland_appimage "$root/wayland-appimage.log" \
    env -i "${wayland_env[@]}" \
      timeout 45s "$candidate" --appimage-extract-and-run --smoke-test
  if [[ "$captured_status" != 0 ]]; then
    failure_status=$captured_status
    diagnose_wayland_failure wayland_appimage "$failure_status"
    exit "$failure_status"
  fi
)
pass_stage
kill "$weston_pid" 2>/dev/null || true
wait "$weston_pid" 2>/dev/null || true
unset weston_pid
kill "$xvfb_pid" 2>/dev/null || true
wait "$xvfb_pid" 2>/dev/null || true
unset xvfb_pid

if find "$root/cwd" -mindepth 1 -print -quit | grep -q .; then
  echo 'application wrote into foreign working directory after display probes' >&2
  find "$root/cwd" -mindepth 1 -maxdepth 2 -print >&2
  exit 1
fi
printf 'foreign_cwd_final_clean=PASS\n' >> "$report"
