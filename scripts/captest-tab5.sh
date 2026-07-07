#!/usr/bin/env bash
# Build + flash + monitor the Tab5 RADIO CAPABILITY PROBE (diagnostic firmware).
# This temporarily replaces the normal app with a one-shot probe that logs which
# offensive-radio primitives the C6 exposes over esp_hosted. Reflash the normal
# firmware afterwards with: ./scripts/flash-tab5.sh
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ -z "${IDF_PATH:-}" ]]; then
  if [[ -d "${ROOT_DIR}/../esp-idf-5.4.2" ]]; then
    export IDF_PATH="${ROOT_DIR}/../esp-idf-5.4.2"
  elif [[ -d "${ROOT_DIR}/../esp-idf" ]]; then
    export IDF_PATH="${ROOT_DIR}/../esp-idf"
  fi
fi

if [[ -d "${ROOT_DIR}/../.espressif-542" ]]; then
  export IDF_TOOLS_PATH="${ROOT_DIR}/../.espressif-542"
fi

if [[ -z "${IDF_PYTHON_ENV_PATH:-}" && -n "${IDF_TOOLS_PATH:-}" ]]; then
  for candidate in \
    "${IDF_TOOLS_PATH}/python_env/idf5.4_py3.9_env" \
    "${IDF_TOOLS_PATH}/python_env/idf5.4_py3.12_env" \
    "${IDF_TOOLS_PATH}/python_env/idf5.4_py3.11_env"; do
    if [[ -x "${candidate}/bin/python" ]]; then
      export IDF_PYTHON_ENV_PATH="${candidate}"
      export PATH="${candidate}/bin:${PATH}"
      break
    fi
  done
fi

if [[ -z "${IDF_PATH:-}" || ! -f "${IDF_PATH}/export.sh" ]]; then
  echo "ESP-IDF no encontrado. Define IDF_PATH o instala ESP-IDF 5.4.2." >&2
  exit 1
fi

# shellcheck disable=SC1090
source "${IDF_PATH}/export.sh"

cd "${ROOT_DIR}/platforms/tab5"
idf.py -DSPECTRA5_RADIO_CAPTEST=1 build "$@"
idf.py flash monitor
