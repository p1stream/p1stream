#!/bin/bash
set -e

OUT_APP=out/P1stream.app
OUT_CONTENTS=$OUT_APP/Contents
OUT_BIN=$OUT_CONTENTS/MacOS
OUT_RES=$OUT_CONTENTS/Resources

rm -fr $OUT_APP
mkdir -p $OUT_BIN $OUT_RES

cp -r mac/Info.plist $OUT_CONTENTS/
cp -r out/node out/core.node $OUT_BIN/
cp -r lib $OUT_RES/
