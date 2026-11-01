#!/usr/bin/env bash
set -euo pipefail
app_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$app_root/scripts/env.sh"
app_binary="$app_root/build/dev/holzwerkprojekt"
if [[ ! -x "$app_binary" ]]; then
  printf 'Build Woodworking Projects first with scripts/run.sh.\n' >&2
  exit 1
fi
exec "$app_binary" "$@"
