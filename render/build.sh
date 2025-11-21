#!/bin/bash
# Build script for the renderer

set -e

echo "Building renderer..."
make clean
make

echo ""
echo "Build successful!"
echo ""
echo "Try running:"
echo "  ./renderer                          # Render Cornell box test scene"
echo "  ./renderer -i teapot.obj            # Render pyramid from OBJ"
echo "  ./renderer --help                   # Show all options"
