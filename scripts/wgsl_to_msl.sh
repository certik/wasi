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

    python3 - "${tmp_out}" <<'PY'
from pathlib import Path
import sys

tmp_path = Path(sys.argv[1])
base = tmp_path.stem
text = tmp_path.read_text()

# Fix buffer bindings emitted as fake user attributes
for idx in range(8):
    text = text.replace(f"[[user(fake{idx})]]", f"[[buffer({idx})]]")

# Determine stage and target entrypoint name
entry = None
if base.endswith('_vertex'):
    if '_overlay_' in base:
        entry = 'overlay_vertex'
    else:
        entry = 'main_vertex'
    text = text.replace('vertex main_Output main_(', f'vertex main_Output {entry}(')
elif base.endswith('_fragment'):
    if '_overlay_' in base:
        entry = 'overlay_fragment'
    else:
        entry = 'main_fragment'
    text = text.replace('fragment main_Output main_(', f'fragment main_Output {entry}(')

tmp_path.write_text(text)
PY

    mv "${tmp_out}" "${dest}"
done
