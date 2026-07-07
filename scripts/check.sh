#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"${ROOT_DIR}/scripts/format.sh"
"${ROOT_DIR}/scripts/build-desktop.sh"
"${ROOT_DIR}/scripts/test.sh"

if [[ "${SKIP_TAB5_BUILD:-0}" != "1" ]]; then
  if "${ROOT_DIR}/scripts/build-tab5.sh"; then
    echo "Tab5 build OK"
  else
    echo "Tab5 build falló (entorno ESP-IDF incompleto). Define SKIP_TAB5_BUILD=1 para omitir." >&2
    exit 1
  fi
fi

echo "check.sh completado."
