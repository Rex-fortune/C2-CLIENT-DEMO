#!/usr/bin/env bash
set -euo pipefail

# Cross-compile Windows executable (mingw-w64)
cd "$(dirname "$0")/src"

x86_64-w64-mingw32-g++ *.cpp -o ../build/client.exe -std=gnu++17 \
    -lws2_32 -lgdiplus -lgdi32 -lole32 -luuid -lstrmiids \
    -static-libgcc -static-libstdc++

echo "Build complete: ../build/client.exe"
