#!/bin/bash
set -e

GENFLAGS=
CLEAN=0
BUILD=1
RUN=0

while getopts "hcCdrR" opt; do case $opt in
h)
    echo "Usage: $0 [-hcCdrR]"
    echo " -h: print usage then exit"
    echo " -c: clean output before build"
    echo " -C: clean output then exit"
    echo " -d: build with debug flags"
    echo " -r: run after build"
    echo " -R: run with lldb after build"
    exit 64
    ;;
c) CLEAN=1 ;;
C) CLEAN=1; BUILD=0 ;;
d) GENFLAGS=--debug ;;
r) RUN=1 ;;
R) RUN=2 ;;
esac done

cd "$(dirname $0)"

if [ "$CLEAN" == "1" ]; then
    echo "  CLEAN out/"
    rm -fr out/
fi

if [ "$BUILD" == "0" ]; then
    exit
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

tools/gen.py $GENFLAGS > out/build.ninja
out/ninja -f out/build.ninja

if [ "$RUN" == "1" ]; then
    out/P1stream.app/Contents/MacOS/P1stream
elif [ "$RUN" == "2" ]; then
    lldb -- out/P1stream.app/Contents/MacOS/P1stream
fi
