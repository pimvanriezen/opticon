#!/bin/bash
exitfail() {
  echo "$*" >&2
  exit 1
}

# Make sure we're root
[ $UID -eq 0 ] || exitfail Not root

# Set up build dir
VERSION=$(git describe --tags | cut -f1 -d' ' | cut -f1-2 -d-)

# Allow version override
if [ "$1" = "--version" ]; then
  VERSION="$2"
  [ -z "$VERSION" ] && exitfail Argument error on --version
fi

BUILDROOT=/var/build/opticon-agent_$VERSION

[ -d $BUILDROOT ] && rm -rf $BUILDROOT

mkdir -p $BUILDROOT || exitfail Could not create build dir
mkdir -p $BUILDROOT/etc/opticon
mkdir -p $BUILDROOT/usr/sbin
mkdir -p $BUILDROOT/DEBIAN

# Create debian control file
cat > $BUILDROOT/DEBIAN/control << _EOF_
Package: opticon-agent
Version: $VERSION
Section: base
Priority: optional
Architecture: amd64
Maintainer: NewVM <info@newvm.com>
Description: Opticon Agent Software
 Gathers local performance information to send to an Opticon collector.
_EOF_

# Create debian post-install script
cp pkg/debian-postinst.sh $BUILDROOT/DEBIAN/postinst

# Copy binaries
cp bin/opticon-agent $BUILDROOT/usr/sbin/
chmod 750 $BUILDROOT/usr/sbin/opticon-agent

# Build the package
dpkg-deb --build $BUILDROOT || exitfail Could not build
rm -rf $BUILDROOT
mkdir -p pkg/deb
cp ${BUILDROOT}.deb pkg/deb/
