#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCENARIO=""
FULLSCREEN=0
SCALE=""
SCREEN=""
SCREENSHOT=""
EXIT_AFTER_SCREENSHOT=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --scenario) SCENARIO="$2"; shift 2 ;;
    --fullscreen) FULLSCREEN=1; shift ;;
    --scale) SCALE="$2"; shift 2 ;;
    --screen) SCREEN="$2"; shift 2 ;;
    --screenshot) SCREENSHOT="$2"; shift 2 ;;
    --exit-after-screenshot) EXIT_AFTER_SCREENSHOT=1; shift ;;
    *) echo "Opción desconocida: $1" >&2; exit 1 ;;
  esac
done

CANDIDATES=(
  "${ROOT_DIR}/build_desktop/desktop/app_desktop_build"
  "${ROOT_DIR}/build/desktop/app_desktop_build"
  "${ROOT_DIR}/build-desktop/desktop/app_desktop_build"
)

BINARY=""
for candidate in "${CANDIDATES[@]}"; do
  if [[ -x "${candidate}" ]]; then
    BINARY="${candidate}"
    break
  fi
done

if [[ -z "${BINARY}" ]]; then
  echo "No se encontró app_desktop_build. Ejecuta ./scripts/build-desktop.sh" >&2
  exit 1
fi

export SPECTRA5_SCENARIO="${SCENARIO}"
export SPECTRA5_FULLSCREEN="${FULLSCREEN}"
export SPECTRA5_SCALE="${SCALE}"
export SPECTRA5_SCREEN="${SCREEN}"
export SPECTRA5_SCREENSHOT="${SCREENSHOT}"
export SPECTRA5_EXIT_AFTER_SCREENSHOT="${EXIT_AFTER_SCREENSHOT}"

echo "Ejecutando: ${BINARY}"
exec "${BINARY}"
