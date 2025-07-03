#!/bin/bash

set -ex

clang \
    w2.c stdlib/string.c \
    -target wasm32 \
    -Istdlib/ -Iwasi/ \
    -nostdinc \
    -nostdlib \
    -Wl,--no-entry \
    -Wl,--export=__original_main \
    -o w2.wasm

wasmtime --invoke __original_main w2.wasm
