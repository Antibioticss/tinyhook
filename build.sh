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

local build_arg=(TARGET=$target)

if [[ $sysver != "" ]] {
    build_arg+=(MIN_OSVER=$sysver)
}

if (($compact == 1)) {
    build_arg+=(COMPACT=1)
}

for i ($archs2build) {
    (set -x # print command before executing
        mkdir -p build/$i
        make clean ARCH=$i && make all ARCH=$i $build_arg
        cp libtinyhook.a libtinyhook.dylib build/$i
    )
}

if (($#archs2build > 1)) {
    (set -x
        cd build
        mkdir -p universal
        rm -f universal/libtinyhook.a universal/libtinyhook.dylib
        lipo -create -o universal/libtinyhook.a ${archs2build/%//libtinyhook.a}
        lipo -create -o universal/libtinyhook.dylib ${archs2build/%//libtinyhook.dylib}
        cp universal/libtinyhook.a universal/libtinyhook.dylib ..
    )
}
