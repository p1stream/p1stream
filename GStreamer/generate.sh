#!/bin/bash
set -e -x

# To get consistent builds, files generated by the original build system are
# also committed. This script executes the build system and copies those files
# to `generated/`.

# Running this script is only necessary when updating the dependency in the
# repository, and it's rather much dependant on the environment.

# Updating the dependency involves updating the submodule, running this script,
# and carefully reviewing submodule changes for things that need to be
# reflected in the project build settings.


# GStreamer version we're targetting. Should match the submodule.
VERSION="1.0.6"


# Create scratch directory.
SCRATCH="`dirname $0`/../_scratch"
mkdir -p $SCRATCH
cd $SCRATCH

# Download the tarball.
BASENAME="gstreamer-${VERSION}"
TARBALL="${BASENAME}.tar.xz"
if [ ! -e "${TARBALL}" ]; then
    wget "http://gstreamer.freedesktop.org/src/gstreamer/${TARBALL}"
fi

# Unpack (and trash old scratch dir)
rm -fr "${BASENAME}"
tar -xJf "${TARBALL}"
cd "${BASENAME}"

# Run configure and make on specific files.
./configure \
    --enable-shared \
    --disable-static \
    --disable-nls \
    --disable-parse \
    --disable-option-parsing \
    --disable-registry \
    --disable-examples \
    --disable-tests \
    --disable-benchmarks \
    --disable-tools \
    --disable-check \
    --disable-gtk-doc \
    --disable-docbook
make -C gst gstenumtypes.{c,h}

# Copy generated files.
OUT="../../GStreamer/generated"
mkdir -p "${OUT}/gst/"
cp config.h "${OUT}/"
cp gst/gstconfig.h gst/gstversion.h "${OUT}/gst/"
cp gst/gstenumtypes.{c,h} "${OUT}/gst/"
