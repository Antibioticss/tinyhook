#!/bin/zsh

set -e
set -o pipefail

cd "$(dirname "$0")"

MIN_OS="10.15"

make clean
make build ARCH=x86_64 OSX_VER=$MIN_OS #COMPACT=1
mv libtinyhook.a libtinyhook.a.x86_64
mv libtinyhook.dylib libtinyhook.dylib.x86_64
make clean
make build ARCH=arm64 OSX_VER=$MIN_OS #COMPACT=1
mv libtinyhook.a libtinyhook.a.arm64
mv libtinyhook.dylib libtinyhook.dylib.arm64
make clean

lipo -create libtinyhook.a.x86_64 libtinyhook.a.arm64 -o libtinyhook.a
rm libtinyhook.a.x86_64 libtinyhook.a.arm64
lipo -create libtinyhook.dylib.x86_64 libtinyhook.dylib.arm64 -o libtinyhook.dylib
rm libtinyhook.dylib.x86_64 libtinyhook.dylib.arm64

zip -j -9 tinyhook.zip libtinyhook.a libtinyhook.dylib include/tinyhook.h

echo
echo built universal package: tinyhook.zip libtinyhook.a libtinyhook.dylib
