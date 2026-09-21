#!/usr/bin/env bash
set -euo pipefail

commit=${SUDOKURA_SOURCE_COMMIT:-}
if [[ -z "$commit" ]] && command -v git >/dev/null 2>&1; then
  commit=$(git rev-parse HEAD)
fi

if [[ ! "$commit" =~ ^[0-9a-f]{40}$ ]]; then
  echo "exact source commit is unavailable or invalid: '${commit:-<empty>}'" >&2
  exit 1
fi

printf '%s\n' "$commit"
