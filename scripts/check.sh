#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/env.sh
woodworking_preset="${1:-dev}"
woodworking_files=()
while IFS= read -r -d '' woodworking_file; do
  woodworking_files+=("$woodworking_file")
done < <(find src tests -type f \( -name '*.h' -o -name '*.cpp' \) -print0)
clang-format --dry-run --Werror "${woodworking_files[@]}"
cmake --preset "$woodworking_preset"
cmake --build --preset "$woodworking_preset" --parallel 4
ctest --preset "$woodworking_preset"
if [[ "$woodworking_preset" != asan ]]; then
  if command -v clang-tidy >/dev/null 2>&1; then
    woodworking_sources=()
    while IFS= read -r -d '' woodworking_source; do
      woodworking_sources+=("$woodworking_source")
    done < <(find src -name '*.cpp' -print0)
    clang-tidy -p "build/$woodworking_preset" "${woodworking_sources[@]}" --quiet
  elif [[ "$(uname -s)" == Darwin ]]; then
    python3 scripts/analyze_macos.py "build/$woodworking_preset/compile_commands.json"
  else
    echo "clang-tidy is required for static analysis" >&2
    exit 1
  fi
fi
git diff --check
