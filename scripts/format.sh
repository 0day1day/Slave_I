#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if command -v clang-format >/dev/null 2>&1; then
  find "${ROOT_DIR}/app" "${ROOT_DIR}/platforms" "${ROOT_DIR}/tests" \
    \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.c' \) \
    -print0 | xargs -0 clang-format -i
else
  echo "clang-format no encontrado; omitiendo formato." >&2
fi
