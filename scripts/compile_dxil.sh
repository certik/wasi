#!/usr/bin/env bash
# Compile HLSL shaders to DXIL using DXC compiler
# Works with both bash and shell (prefix-dev/shell)

set -ex

# Path to DXC compiler (adjust if needed)
DXC="C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/dxc.exe"

# Compile vertex shaders
"$DXC" -T vs_6_0 -E main -Fo shaders/DXIL/mousecircle_scene_vertex.dxil shaders/HLSL/mousecircle_scene_vertex.hlsl
"$DXC" -T vs_6_0 -E main -Fo shaders/DXIL/mousecircle_overlay_vertex.dxil shaders/HLSL/mousecircle_overlay_vertex.hlsl

# Compile fragment shaders (pixel shaders in D3D terminology)
"$DXC" -T ps_6_0 -E main -Fo shaders/DXIL/mousecircle_scene_fragment.dxil shaders/HLSL/mousecircle_scene_fragment.hlsl
"$DXC" -T ps_6_0 -E main -Fo shaders/DXIL/mousecircle_overlay_fragment.dxil shaders/HLSL/mousecircle_overlay_fragment.hlsl

echo "Successfully compiled all DXIL shaders"
