#!/bin/zsh

# a simple build script for tinyhook
# arguments:
#   -a <arch>     add an arch to build
#   -t <target>   specify target system, macosx(default) or iphoneos
#   -v <version>  specify minimum system version
#   -c            build compact version

set -e
set -o pipefail

cd "$(dirname "$0")"

archs2build=()
target=macosx
sysver=
compact=0

while {getopts a:t:v:c arg} {
    case $arg {
        (a)
        archs2build+=($OPTARG)
        ;;
        (t)
        target=$OPTARG
        ;;
        (v)
        sysver=$OPTARG
        ;;
        (c)
        compact=1
        ;;
        (?)
        echo error
        return 1
        ;;
    }
}

if (($#archs2build == 0)) {
    if [[ $target == macosx ]] {
        archs2build+=(arm64 x86_64)
    } elif [[ $target == iphoneos ]] {
        archs2build+=(arm64 arm64e)
    } else {
        echo "unknown target: $target!"
        exit 1
    }
}

if [[ $sysver == "" ]] {
    if [[ $target == macosx ]] {
        sysver=10.15
    } elif [[ $target == iphoneos ]] {
        sysver=12.0
    } else {
        echo "unknown target: $target!"
        exit 1
    }
}

local build_arg=(TARGET=$target MIN_OSVER=$sysver)

if (($compact == 1)) {
    build_arg+=(COMPACT=1)
}

for i ($archs2build) {
    mkdir -p build/$i
    make clean ARCH=$i && make all ARCH=$i $build_arg
    mv libtinyhook.a libtinyhook.dylib build/$i
}

if (($#archs2build > 1)) {
    rm -rf build/universal
    mkdir -p build/universal
    lipo -create -o build/universal/libtinyhook.a build/*/libtinyhook.a
    lipo -create -o build/universal/libtinyhook.dylib build/*/libtinyhook.dylib
    cp build/universal/libtinyhook.a build/universal/libtinyhook.dylib .
}
