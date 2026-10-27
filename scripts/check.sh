#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source scripts/env.sh
holzwerk_preset="${1:-dev}"
mapfile -d '' holzwerk_files < <(find src tests -type f \( -name '*.h' -o -name '*.cpp' \) -print0)
clang-format --dry-run --Werror "${holzwerk_files[@]}"
cmake --preset "$holzwerk_preset"
cmake --build --preset "$holzwerk_preset" --parallel 4
ctest --preset "$holzwerk_preset"
if [[ "$holzwerk_preset" != asan ]]; then
  mapfile -d '' holzwerk_sources < <(find src -name '*.cpp' -print0)
  clang-tidy -p "build/$holzwerk_preset" "${holzwerk_sources[@]}" --quiet
fi
git diff --check
