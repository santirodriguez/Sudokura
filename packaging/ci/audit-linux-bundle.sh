#!/usr/bin/env bash
set -euo pipefail

appdir=${1:-AppDir}
test -d "$appdir"
test -x "$appdir/usr/bin/sudokura"

deps_report=dependencies-linux-all.txt
external_report=external-dependencies-linux.txt
abi_report=abi-linux.txt
components_report=components-linux.txt
: > "$deps_report"
: > "$external_report"
: > "$abi_report"
printf 'file\tpackage\tversion\tcopyright\n' > "$components_report"

elfs=()
while IFS= read -r -d '' file_path; do
  if file -b "$file_path" | grep -q '^ELF '; then
    elfs+=("$file_path")
  fi
done < <(find "$appdir" -type f -print0)
(( ${#elfs[@]} > 0 ))

for file_path in "${elfs[@]}"; do
  rel=${file_path#"$appdir/"}
  machine=$(readelf -h "$file_path" | awk -F: '/Machine:/{gsub(/^[[:space:]]+/,"",$2); print $2}')
  printf '[%s]\nmachine=%s\n' "$rel" "$machine" >> "$abi_report"
  grep -Eq 'X86-64|Advanced Micro Devices X86-64' <<<"$machine"

  readelf --version-info "$file_path" 2>/dev/null     | grep -oE 'GLIBC_[0-9]+\.[0-9]+'     | sort -Vu     | sed 's/^/glibc=/' >> "$abi_report" || true

  if readelf -d "$file_path" 2>/dev/null | grep -q '(NEEDED)'; then
    {
      printf '\n[%s]\n' "$rel"
      LD_LIBRARY_PATH="$PWD/$appdir/usr/lib:$PWD/$appdir/usr/bin" ldd "$file_path"
    } >> "$deps_report"
  fi
done

if grep -q 'not found' "$deps_report"; then
  cat "$deps_report" >&2
  echo 'unresolved ELF dependency in AppImage payload' >&2
  exit 1
fi

grep -hoE 'GLIBC_[0-9]+\.[0-9]+' "$abi_report"   | cut -d_ -f2 | sort -Vu | tail -1 > .max-glibc-linux
max_glibc=$(cat .max-glibc-linux)
rm -f .max-glibc-linux
python3 - "$max_glibc" <<'PY'
import sys
parts=lambda value: tuple(int(x) for x in value.split("."))
actual=parts(sys.argv[1])
limit=parts("2.35")
if actual > limit:
    raise SystemExit(f"GLIBC requirement {sys.argv[1]} exceeds Ubuntu 22.04 baseline 2.35")
print(f"max_glibc={sys.argv[1]}")
PY
printf 'max_glibc=%s\n' "$max_glibc" >> "$abi_report"

awk '
  /=> \/[^[:space:]]+/ {print $3}
  /^[[:space:]]*\/[^[:space:]]+[[:space:]]+\(0x/ {print $1}
' "$deps_report" | sort -u | while IFS= read -r dep; do
  case "$dep" in
    "$PWD/$appdir"/*) ;;
    *) printf '%s\n' "$dep" ;;
  esac
done > "$external_report"

if grep -E '/home/runner/|/usr/local/|/opt/[^[:space:]]+' "$external_report"; then
  echo 'build-host dependency escaped AppImage closure' >&2
  exit 1
fi

if find "$appdir" -type f -printf '%f\n'   | grep -Ei '(_dri\.so|libGLX_nvidia|libvulkan_(intel|radeon|nouveau)|libnvidia|libcuda)'; then
  echo 'graphics-driver implementation must remain supplied by the target system' >&2
  exit 1
fi

license_root="$appdir/usr/share/doc/sudokura/licenses"
mkdir -p "$license_root"
while IFS= read -r -d '' bundled; do
  base=$(basename "$bundled")
  source=$(ldconfig -p 2>/dev/null | awk -v b="$base" '$1==b {print $NF; exit}')
  if [[ -z "$source" || ! -e "$source" ]]; then
    source=$(find /usr/lib /lib -type f -name "$base" -print -quit 2>/dev/null || true)
  fi
  if [[ -z "$source" || ! -e "$source" ]]; then
    echo "cannot map bundled library to build-host package: $base" >&2
    exit 1
  fi
  source_real=$(readlink -f "$source")
  owner=$(dpkg-query -S "$source" 2>/dev/null | head -1 || true)
  if [[ -z "$owner" && "$source_real" != "$source" ]]; then
    owner=$(dpkg-query -S "$source_real" 2>/dev/null | head -1 || true)
  fi
  if [[ -z "$owner" && "$source_real" == /usr/* ]]; then
    merged_path=${source_real#/usr}
    owner=$(dpkg-query -S "$merged_path" 2>/dev/null | head -1 || true)
  fi
  if [[ -z "$owner" ]]; then
    echo "cannot identify Debian package for bundled library: $source_real" >&2
    exit 1
  fi
  package=${owner%%:*}
  package_base=${package%%:*}
  version=$(dpkg-query -W -f='${Version}' "$package")
  copyright="/usr/share/doc/$package_base/copyright"
  if [[ ! -f "$copyright" ]]; then
    echo "missing installed copyright file for $package" >&2
    exit 1
  fi
  mkdir -p "$license_root/$package_base"
  cp "$copyright" "$license_root/$package_base/copyright"
  printf '%s\t%s\t%s\t%s\n' "$base" "$package" "$version" "licenses/$package_base/copyright" >> "$components_report"
done < <(find "$appdir/usr/lib" -type f -name '*.so*' -print0)

{
  head -1 "$components_report"
  tail -n +2 "$components_report" | LC_ALL=C sort -u
} > "$components_report.tmp"
mv "$components_report.tmp" "$components_report"
cp "$components_report" "$appdir/usr/share/doc/sudokura/COMPONENTS.txt"
