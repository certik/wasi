#!/bin/bash

set -ex

clang MouseCircle_standalone.c sdl_mock.c -o MouseCircle_mock \
    -I.

echo "Build successful! Run with ./MouseCircle_mock"
