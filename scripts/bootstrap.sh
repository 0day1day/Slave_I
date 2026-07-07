#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

command -v cmake >/dev/null 2>&1 || { echo "cmake no encontrado" >&2; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "python3 no encontrado" >&2; exit 1; }

if ! command -v sdl2-config >/dev/null 2>&1; then
  echo "SDL2 no encontrado. Instala SDL2 antes de continuar." >&2
  exit 1
fi

mkdir -p "${ROOT_DIR}/build" "${ROOT_DIR}/build_desktop" "${ROOT_DIR}/build/tests"

if [[ ! -d "${ROOT_DIR}/dependencies/lvgl" ]]; then
  echo "Descargando dependencias..."
  python3 "${ROOT_DIR}/fetch_repos.py"
fi

JSON_HDR="${ROOT_DIR}/dependencies/json/nlohmann/json.hpp"
if [[ ! -f "${JSON_HDR}" ]]; then
  echo "Descargando nlohmann/json..."
  mkdir -p "$(dirname "${JSON_HDR}")"
  curl -fsSL -o "${JSON_HDR}" \
    "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp"
fi

AUDIO_DIR="${ROOT_DIR}/platforms/tab5/audio"
if [[ ! -f "${AUDIO_DIR}/startup_sfx.mp3" ]]; then
  echo "Descargando assets de audio Tab5..."
  mkdir -p "${AUDIO_DIR}"
  base="https://raw.githubusercontent.com/m5stack/M5Tab5-UserDemo/main/platforms/tab5/audio"
  curl -fsSL -o "${AUDIO_DIR}/canon_in_d.mp3" "${base}/canon_in_d.mp3"
  curl -fsSL -o "${AUDIO_DIR}/startup_sfx.mp3" "${base}/startup_sfx.mp3"
  curl -fsSL -o "${AUDIO_DIR}/shutdown_sfx.mp3" "${base}/shutdown_sfx.mp3"
fi

echo "Bootstrap completado."
