#!/bin/sh
# Explicit, checksum-pinned local dependency build; never runs during configure.
set -eu
if [ "$#" -ne 1 ]; then echo "Usage: sh tools/bootstrap-sdl3.sh OUTPUT_DIRECTORY" >&2; exit 2; fi
mkdir -p "$1"
output=$(CDPATH= cd -- "$1" && pwd)
archive="$output/SDL3-3.4.12.tar.gz"
digest=f07b958a9ac5020fb7a44cadb957f658b2149c3c8abb4f63145fac9303249db7
if [ ! -f "$archive" ]; then
    curl -fL --connect-timeout 20 --max-time 180 \
        https://github.com/libsdl-org/SDL/releases/download/release-3.4.12/SDL3-3.4.12.tar.gz \
        -o "$archive.part"
    mv "$archive.part" "$archive"
fi
actual=$(shasum -a 256 "$archive" | cut -d ' ' -f 1)
if [ "$actual" != "$digest" ]; then echo "SDL3 archive checksum mismatch" >&2; exit 1; fi
if [ ! -d "$output/SDL3-3.4.12" ]; then tar -xzf "$archive" -C "$output"; fi
cmake -S "$output/SDL3-3.4.12" -B "$output/sdl-build" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$output/install" \
    -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF
cmake --build "$output/sdl-build" --parallel 4
cmake --install "$output/sdl-build"
printf 'SDL3 installed at %s\n' "$output/install"
