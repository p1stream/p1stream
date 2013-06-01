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


# gst-plugins-base version we're targetting. Should match the submodule.
VERSION="1.0.6"


# Create scratch directory.
SCRATCH="`dirname $0`/../_scratch"
mkdir -p $SCRATCH
cd $SCRATCH

# Download the tarball.
BASENAME="gst-plugins-base-${VERSION}"
TARBALL="${BASENAME}.tar.xz"
if [ ! -e "${TARBALL}" ]; then
    wget "http://gstreamer.freedesktop.org/src/gst-plugins-base/${TARBALL}"
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
    --disable-gtk-doc \
    --disable-examples \
    --disable-introspection \
    --enable-experimental \
    --enable-orc \
    --without-x
make -C gst-libs/gst/audio audio-enumtypes.{c,h}
make -C gst-libs/gst/pbutils pbutils-enumtypes.{c,h}
make -C gst-libs/gst/video video-enumtypes.{c,h}

# Copy generated files.
OUT="../../gst-plugins-base/generated"
mkdir -p \
    "${OUT}/gst-libs/gst/audio" \
    "${OUT}/gst-libs/gst/pbutils" \
    "${OUT}/gst-libs/gst/video"
cp config.h _stdint.h "${OUT}/"
cp gst-libs/gst/audio/audio-enumtypes.{c,h} "${OUT}/gst-libs/gst/audio/"
cp gst-libs/gst/pbutils/gstpluginsbaseversion.h "${OUT}/gst-libs/gst/pbutils/"
cp gst-libs/gst/pbutils/pbutils-enumtypes.{c,h} "${OUT}/gst-libs/gst/pbutils/"
cp gst-libs/gst/video/video-enumtypes.{c,h} "${OUT}/gst-libs/gst/video/"
