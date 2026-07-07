#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"${ROOT_DIR}/scripts/build-desktop.sh" "$@"
exec "${ROOT_DIR}/scripts/run-desktop.sh" "$@"
