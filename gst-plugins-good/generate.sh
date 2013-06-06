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


# gst-plugins-good version we're targetting. Should match the submodule.
VERSION="1.0.7"


# Create scratch directory.
SCRATCH="`dirname $0`/../_scratch"
mkdir -p $SCRATCH
cd $SCRATCH

# Download the tarball.
BASENAME="gst-plugins-good-${VERSION}"
TARBALL="${BASENAME}.tar.xz"
if [ ! -e "${TARBALL}" ]; then
    wget "http://gstreamer.freedesktop.org/src/gst-plugins-good/${TARBALL}"
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
    --disable-examples \
    --enable-experimental \
    --enable-orc \
    --disable-gtk-doc \
    --without-x

# Copy generated files.
OUT="../../gst-plugins-good/generated"
mkdir -p "${OUT}/"
cp _stdint.h config.h "${OUT}/"