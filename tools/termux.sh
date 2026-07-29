#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

for cmd in clang cmake ninja; do
    if ! command -v "${cmd}" >/dev/null 2>&1; then
        printf -- "-  SETUP       pkg install clang cmake ninja\n"
        pkg update -y >/dev/null
        pkg install -y clang cmake ninja
        break
    fi
done

ARCH=$(uname -m)

case "${ARCH}" in
    aarch64|arm64)
        ABI="arm64-v8a"
    ;;
    armv7l|armv8l|arm)
        ABI="armeabi-v7a"
    ;;
    *)
        printf -- "-  ERR         unsupported arch: %s\n" "${ARCH}" >&2
        exit 1
    ;;
esac

BUILD_DIR="build/${ABI}/release"

printf -- "-  CONF        %s\n" "${BUILD_DIR}"
cmake -B "${BUILD_DIR}" -G Ninja -DCMAKE_BUILD_TYPE=Release >/dev/null

printf -- "-  BUILD       %s\n" "${BUILD_DIR}"
cmake --build "${BUILD_DIR}"