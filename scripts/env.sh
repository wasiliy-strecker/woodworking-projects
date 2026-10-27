#!/usr/bin/env bash
# Optional machine-local SDK paths. No dependency on another application.
holzwerk_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ -f "$holzwerk_root/.tools/env.sh" ]]; then
  source "$holzwerk_root/.tools/env.sh"
fi
if [[ -n "${HOLZWERKPROJEKT_QT_ROOT:-}" ]]; then
  export CMAKE_PREFIX_PATH="$HOLZWERKPROJEKT_QT_ROOT${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
  export PATH="$HOLZWERKPROJEKT_QT_ROOT/bin:$PATH"
fi
