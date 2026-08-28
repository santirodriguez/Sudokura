#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
HEADER="$ROOT/version.h"

read_macro() {
  awk -v name="$1" '$1 == "#define" && $2 == name { print $3; found=1; exit } END { if (!found) exit 1 }' "$HEADER"
}

major=$(read_macro SUDOKURA_VERSION_MAJOR)
minor=$(read_macro SUDOKURA_VERSION_MINOR)
patch=$(read_macro SUDOKURA_VERSION_PATCH)

for value in "$major" "$minor" "$patch"; do
  case "$value" in
    ''|*[!0-9]*)
      echo "invalid numeric version component in $HEADER: $value" >&2
      exit 1
      ;;
  esac
done

printf '%s.%s.%s\n' "$major" "$minor" "$patch"
