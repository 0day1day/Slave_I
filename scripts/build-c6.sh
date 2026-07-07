#!/usr/bin/env bash
# Build the ESP32-C6 radio-engine firmware (esp_hosted slave + Spectra5 offensive
# module). Target: esp32c6. Output: network_adapter.bin (OTA-pushable to the C6
# from the P4 via esp_hosted_slave_ota; no cable). See docs/architecture/.
#
# NOTE: the C6 slave is currently built in place inside the pinned esp_hosted
# component (examples/slave) because its sources use tree-relative paths
# (../../common). esp_hosted is pinned at 1.4.0, so this is stable; a clean
# vendored platforms/c6/ project is a follow-up.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SLAVE_DIR="${ROOT_DIR}/platforms/tab5/managed_components/espressif__esp_hosted/examples/slave"

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

cd "${SLAVE_DIR}"
idf.py set-target esp32c6 build "$@"
echo
echo "C6 firmware: ${SLAVE_DIR}/build/network_adapter.bin"
