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


# GLib version we're targetting. Should match the submodule.
VERSION="2.36.2"

# Homebrew installed versions of required deps, needed to generate.
LIBFFI_VERSION="3.0.13"
GETTEXT_VERSION="0.18.2"


# Create scratch directory.
SCRATCH="`dirname $0`/../_scratch"
mkdir -p $SCRATCH
cd $SCRATCH

# Download the tarball.
BASENAME="glib-${VERSION}"
TARBALL="${BASENAME}.tar.xz"
if [ ! -e "${TARBALL}" ]; then
    SERIES=`echo ${VERSION} | awk '{ split($0, a, "."); printf("%d.%d", a[1], a[2]) }'`
    wget "http://ftp.gnome.org/pub/gnome/sources/glib/${SERIES}/${TARBALL}"
fi

# Unpack (and trash old scratch dir)
rm -fr "${BASENAME}"
tar -xJf "${TARBALL}"
cd "${BASENAME}"

# Run configure.
export \
    PKG_CONFIG_PATH="/usr/local/Cellar/libffi/${LIBFFI_VERSION}/lib/pkgconfig" \
    CFLAGS="-I/usr/local/Cellar/gettext/${GETTEXT_VERSION}/include" \
    LDFLAGS="-L/usr/local/Cellar/gettext/${GETTEXT_VERSION}/lib" \
    PATH="$PATH:/usr/local/Cellar/gettext/${GETTEXT_VERSION}/bin"
./configure \
    --enable-shared \
    --disable-static \
    --disable-gtk-doc \
    --disable-man

# Copy generated files.
OUT="../../GLib/generated"
mkdir -p \
    "${OUT}/glib/" \
    "${OUT}/gmodule/"
cp config.h "${OUT}/"
cp glib/glibconfig.h "${OUT}/glib/"
cp gmodule/gmoduleconf.h "${OUT}/gmodule/"
