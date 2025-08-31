#!/bin/zsh

set -e
set -o pipefail

cd "$(dirname "$0")"

MIN_OSX="10.15"
MIN_IOS="12.0"

function build_macos() {
    mkdir arm64 x86_64
    make clean
    make ARCH=x86_64 OSX_VER=$MIN_OSX # COMPACT=1
    mv libtinyhook.a libtinyhook.dylib x86_64
    make clean
    make ARCH=arm64 OSX_VER=$MIN_OSX # COMPACT=1
    mv libtinyhook.a libtinyhook.dylib arm64
    make clean

    lipo -create x86_64/libtinyhook.a arm64/libtinyhook.a -o libtinyhook.a
    lipo -create x86_64/libtinyhook.dylib arm64/libtinyhook.dylib -o libtinyhook.dylib
    rm -r arm64 x86_64
}

function build_ios() {
    mkdir arm64 arm64e
    make clean
    make ARCH=arm64e TARGET=iphoneos IOS_VER=$MIN_IOS # COMPACT=1
    mv libtinyhook.a libtinyhook.dylib arm64e
    make clean
    make ARCH=arm64 TARGET=iphoneos IOS_VER=$MIN_IOS # COMPACT=1
    mv libtinyhook.a libtinyhook.dylib arm64
    make clean

    lipo -create arm64e/libtinyhook.a arm64/libtinyhook.a -o libtinyhook.a
    lipo -create arm64e/libtinyhook.dylib arm64/libtinyhook.dylib -o libtinyhook.dylib
    rm -r arm64 arm64e
}

target=$1

if [[ $target == "macos" || $target == "" ]] {
    build_macos
} elif [[ $target == "ios" ]] {
    build_ios
} else {
    echo "unknown target!"
    exit 1
}
