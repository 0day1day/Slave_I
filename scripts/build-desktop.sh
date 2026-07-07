#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_TYPE="Debug"
BUILD_DIR="${ROOT_DIR}/build_desktop"
CLEAN=0
ASAN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --clean) CLEAN=1; shift ;;
    --release) BUILD_TYPE="Release"; shift ;;
    --asan) ASAN=1; shift ;;
    *) echo "Opción desconocida: $1" >&2; exit 1 ;;
  esac
done

"${ROOT_DIR}/scripts/bootstrap.sh"

if [[ "${CLEAN}" -eq 1 ]]; then
  rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"

CMAKE_ARGS=(
  -S "${ROOT_DIR}"
  -B "${BUILD_DIR}"
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
)

if [[ "${ASAN}" -eq 1 ]]; then
  CMAKE_ARGS+=(-DSANITIZE_ADDRESS=ON)
fi

cmake "${CMAKE_ARGS[@]}"
cmake --build "${BUILD_DIR}" -j8

echo "Binario: ${ROOT_DIR}/build/desktop/app_desktop_build"
