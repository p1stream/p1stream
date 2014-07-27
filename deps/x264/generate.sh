#!/bin/bash

# To get consistent builds, files generated by the original build system are
# also committed. This script executes the build system and copies those files
# to `generated/`.

# Running this script is only necessary when updating the dependency in the
# repository, and it's rather much dependant on the environment.

# Updating the dependency involves updating the submodule, running this script,
# and carefully reviewing submodule changes for things that need to be
# reflected in the project build settings.

if [ $# -ne 1 ]; then
    echo "Usage: ${0} <gyp platform>"
    exit 64
fi

# Create scratch directory.
set -e -x
cd `dirname ${0}`

# Create a shared clone of the repo.
rm -fr _scratch
git clone --shared ../../deps/x264/x264 _scratch

# Run configure.
pushd _scratch
./configure \
    --disable-cli \
    --enable-shared
make common/oclobj.h
popd

# Copy generated files.
mkdir -p "generated/${1}" generated/common
cp _scratch/config.h _scratch/x264_config.h "generated/${1}/"
cp _scratch/common/oclobj.h generated/common/

# Clean up
rm -fr _scratch
