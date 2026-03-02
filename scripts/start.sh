#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
APP_BIN="${BUILD_DIR}/cpp-spider"

if [[ ! -x "${APP_BIN}" ]]; then
  "${ROOT_DIR}/scripts/build.sh"
fi

exec "${APP_BIN}" "$@"
