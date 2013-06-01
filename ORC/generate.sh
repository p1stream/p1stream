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


# ORC version we're targetting. Should match the submodule.
VERSION="0.4.17"


# Create scratch directory.
SCRATCH="`dirname $0`/../_scratch"
mkdir -p $SCRATCH
cd $SCRATCH

# Download the tarball.
BASENAME="orc-${VERSION}"
TARBALL="${BASENAME}.tar.gz"
if [ ! -e "${TARBALL}" ]; then
    wget "http://code.entropywave.com/download/orc/${TARBALL}"
fi

# Unpack (and trash old scratch dir)
rm -fr "${BASENAME}"
tar -xzf "${TARBALL}"
cd "${BASENAME}"

# Run configure.
./configure --disable-gtk-doc --enable-shared --disable-static --enable-backend=sse

# Selecting just the SSE backend actually breaks the build. :(
sed -i '' -e 's/^.*ENABLE_BACKEND_MMX.*$/#define ENABLE_BACKEND_MMX 1/' config.h

# Copy generated files.
OUT="../../ORC/generated"
mkdir -p "${OUT}/"
cp config.h "${OUT}/"
