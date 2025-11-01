#!/usr/bin/env bash
# Compile every WGSL shader in shaders/ to MSL using naga.

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAGA_BIN="/Users/ondrej/repos/wgpu/target/debug/naga"
SHADER_DIR="${PROJECT_ROOT}/shaders"

if [[ ! -x "${NAGA_BIN}" ]]; then
    echo "naga binary not found or not executable at ${NAGA_BIN}" >&2
    exit 1
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

tmp_dir="$(mktemp -d)"
cleanup() { rm -rf "${tmp_dir}"; }
trap cleanup EXIT

for src in "${wgsl_files[@]}"; do
    base_name="$(basename "${src}" .wgsl)"
    tmp_out="${tmp_dir}/${base_name}.metal"
    dest="${SHADER_DIR}/${base_name}.msl"

    echo "Compiling ${src} -> ${dest}"
    "${NAGA_BIN}" "${src}" "${tmp_out}"
    mv "${tmp_out}" "${dest}"
done
