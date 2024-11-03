#!/bin/zsh

set -e
set -o pipefail

cd "$(dirname "$0")"

MIN_OS="10.15"

make clean
make build ARCH=x86_64 OSX_VER=$MIN_OS
mv libtinyhook.a libtinyhook.a.x86_64
make clean
make build ARCH=arm64 OSX_VER=$MIN_OS
mv libtinyhook.a libtinyhook.a.arm64
make clean

lipo -create libtinyhook.a.x86_64 libtinyhook.a.arm64 -o libtinyhook.a
rm libtinyhook.a.x86_64 libtinyhook.a.arm64
zip -j -9 tinyhook.zip libtinyhook.a include/tinyhook.h

echo
echo built universal package: tinyhook.zip libtinyhook.a
