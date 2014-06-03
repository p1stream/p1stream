#!/bin/bash
set -e

if [ "$1" = "-h" ]; then
    echo "Usage: $0 [ -h | -f | -c ]"
    echo " -h: display usage"
    echo " -f: regenerate build.ninja and build"
    echo " -c: clean output and build from scratch"
    exit 64
fi

cd "$(dirname $0)"

if [ "$1" = "-c" ]; then
    echo "  CLEAN out"
    rm -fr out/
elif [ "$1" = "-f" ]; then
    echo "  CLEAN out/build.ninja"
    rm -f out/build.ninja
fi

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

exec out/ninja -f out/build.ninja
