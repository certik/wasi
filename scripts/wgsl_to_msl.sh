#!/usr/bin/env bash
# Compile every WGSL shader in shaders/ to MSL using our custom wgsl2msl tool.

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WGSL2MSL_BIN="${PROJECT_ROOT}/scripts/target/release/wgsl2msl"
SHADER_DIR="${PROJECT_ROOT}/shaders"

# Build the tool if it doesn't exist
if [[ ! -x "${WGSL2MSL_BIN}" ]]; then
    echo "Building wgsl2msl tool..."
    (cd "${PROJECT_ROOT}/scripts" && cargo build --release)
fi

if [[ ! -d "${SHADER_DIR}" ]]; then
    echo "Shader directory not found: ${SHADER_DIR}" >&2
    exit 1
fi

shopt -s nullglob
wgsl_files=("${SHADER_DIR}"/*.wgsl)

if (( ${#wgsl_files[@]} == 0 )); then
    echo "No WGSL files found in ${SHADER_DIR}" >&2
    exit 0
fi

for src in "${wgsl_files[@]}"; do
    base_name="$(basename "${src}" .wgsl)"
    dest="${SHADER_DIR}/${base_name}.msl"

    echo "Compiling ${src} -> ${dest}"
    "${WGSL2MSL_BIN}" "${src}" "${dest}"
done
