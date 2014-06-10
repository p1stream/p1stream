#!/bin/bash
set -e

CLEAN=none
BUILD=1
RUN=0

while getopts "hbcCr" opt; do case $opt in
h)
    echo "Usage: $0 [-hbcCr]"
    echo " -h: print usage then exit"
    echo " -b: regenerate build.ninja before build"
    echo " -c: clean output before build"
    echo " -C: clean output then exit"
    echo " -r: run after build"
    exit 64
    ;;
b) CLEAN=build ;;
c) CLEAN=all ;;
C) CLEAN=all; BUILD=0 ;;
r) RUN=1 ;;
esac done

cd "$(dirname $0)"

if [ "$CLEAN" == "build" ]; then
    echo "  CLEAN out/build.ninja"
    rm -f out/build.ninja
fi

if [ "$CLEAN" == "all" ]; then
    echo "  CLEAN out/"
    rm -fr out/
fi

if [ "$BUILD" == "1" ]; then

    if [ ! -x "out/ninja" ]; then
        echo "  BUILD out/ninja"

        mkdir -p out/scratch/
        git clone --shared deps/ninja out/scratch/ninja

        cd out/scratch/ninja/
        python bootstrap.py
        cd ../../../

        mv out/scratch/ninja/ninja out/ninja
        rm -fr out/scratch/ninja
    fi

    if [ ! -f "out/build.ninja" ]; then
        echo "  BUILD out/build.ninja"
        tools/gen.py > out/build.ninja
    fi

    out/ninja -f out/build.ninja

    if [ "$RUN" == "1" ]; then
        out/P1stream.app/Contents/MacOS/P1stream
    fi

fi
