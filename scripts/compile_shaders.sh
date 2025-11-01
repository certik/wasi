#!/usr/bin/env bash
# Compile every WGSL shader in shaders/WGSL/ to MSL, HLSL, and SPIR-V.

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SHADER_COMPILER_BIN="${PROJECT_ROOT}/scripts/target/release/shader_compiler"
WGSL_DIR="${PROJECT_ROOT}/shaders/WGSL"

# Build the tool if it doesn't exist
if [[ ! -x "${SHADER_COMPILER_BIN}" ]]; then
    echo "Building shader_compiler tool..."
    (cd "${PROJECT_ROOT}/scripts" && cargo build --release)
fi

if [[ ! -d "${WGSL_DIR}" ]]; then
    echo "WGSL directory not found: ${WGSL_DIR}" >&2
    exit 1
fi

shopt -s nullglob
wgsl_files=("${WGSL_DIR}"/*.wgsl)

if (( ${#wgsl_files[@]} == 0 )); then
    echo "No WGSL files found in ${WGSL_DIR}" >&2
    exit 0
fi

echo "Compiling ${#wgsl_files[@]} shader(s)..."

for src in "${wgsl_files[@]}"; do
    "${SHADER_COMPILER_BIN}" "${src}"
done

echo "Done! Generated shaders in MSL/, HLSL/, and SPIRV/ directories."
