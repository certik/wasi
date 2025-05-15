#!/bin/bash

set -ex

WASI_SDK=$HOME/ext/wasi-sdk-25.0-x86_64-macos
CLANG=$WASI_SDK/bin/clang

$CLANG \
    -O2 -s \
    -Wl,--export=add \
    -Wl,--export=mysin \
    -Wl,--no-entry \
    -nostdlib \
    -o example.wasm \
    example.c

#wasm-objdump -x -j Import example.wasm
#wasm-objdump -x -j Export example.wasm

wasmtime run --invoke mysin example.wasm 1.5
wasmtime run --invoke add example.wasm 5 7
node example.js
