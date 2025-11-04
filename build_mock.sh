#!/bin/bash

set -ex

clang game.c sdl_mock.c -o game_mock \
    -I.

echo "Build successful! Run with ./game_mock"
