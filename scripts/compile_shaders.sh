#!/usr/bin/env bash
# Compile every WGSL shader in shaders/WGSL/ to MSL, HLSL, and SPIR-V.

set -ex

cd scripts
cargo build # --release
cd ..

for shader in shaders/WGSL/*.wgsl; do
    scripts/target/debug/shader_compiler "$shader"
done
