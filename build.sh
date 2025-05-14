#!/bin/bash

set -ex

WASI_SDK=$HOME/ext/wasi-sdk-25.0-x86_64-macos
CLANG=$WASI_SDK/bin/clang

$CLANG \
    -O2 -s \
    -o example.wasm \
    example.c

wasmtime example.wasm
