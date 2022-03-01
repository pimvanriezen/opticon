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

# =============================================================================
# Agent build
# =============================================================================

BUILDROOT=/var/build/opticon-agent_$VERSION

[ -d $BUILDROOT ] && rm -rf $BUILDROOT

mkdir -p $BUILDROOT || exitfail Could not create build dir
mkdir -p $BUILDROOT/etc/opticon
mkdir -p $BUILDROOT/lib/systemd/system
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
cp pkg/opticon-agent.debian-postinst.sh $BUILDROOT/DEBIAN/postinst

# Copy binaries, scripts and example config
cp bin/opticon-agent $BUILDROOT/usr/sbin/
chmod 750 $BUILDROOT/usr/sbin/opticon-agent
cp init/opticon-agent.service $BUILDROOT/lib/systemd/system/
cp src/opticon-agent/opticon-agent.conf.example $BUILDROOT/etc/opticon/

# Build the package
dpkg-deb --build $BUILDROOT || exitfail Could not build
rm -rf $BUILDROOT
mkdir -p pkg/deb
cp ${BUILDROOT}.deb pkg/deb/

# =============================================================================
# Collector build
# =============================================================================

BUILDROOT=/var/build/opticon-collector_$VERSION

[ -d $BUILDROOT ] && rm -rf $BUILDROOT

mkdir -p $BUILDROOT || exitfail Could not create build dir
mkdir -p $BUILDROOT/etc/opticon
mkdir -p $BUILDROOT/var/opticon/db
mkdir -p $BUILDROOT/lib/systemd/system
mkdir -p $BUILDROOT/usr/sbin
mkdir -p $BUILDROOT/var/log/opticon
mkdir -p $BUILDROOT/DEBIAN

# Create debian control file
cat > $BUILDROOT/DEBIAN/control << _EOF_
Package: opticon-collector
Version: $VERSION
Section: base
Priority: optional
Architecture: amd64
Maintainer: NewVM <info@newvm.com>
Description: Opticon Collector Software
 Gathers performance information from remote agents and stores it.
_EOF_

# Create debian post-install script
cp pkg/opticon-collector.debian-postinst.sh $BUILDROOT/DEBIAN/postinst

# Copy binaries, scripts and example config
cp bin/opticon-collector $BUILDROOT/usr/sbin/
chmod 750 $BUILDROOT/usr/sbin/opticon-collector
cp init/opticon-collector.service $BUILDROOT/lib/systemd/system/
cp src/opticon-collector/opticon-collector.conf.example $BUILDROOT/etc/opticon/
cp src/opticon-collector/opticon-meter.conf.example $BUILDROOT/etc/opticon/
cp src/opticon-collector/opticon-graph.conf.example $BUILDROOT/etc/opticon/

# Build the package
dpkg-deb --build $BUILDROOT || exitfail Could not build
rm -rf $BUILDROOT
mkdir -p pkg/deb
cp ${BUILDROOT}.deb pkg/deb/

# =============================================================================
# API build
# =============================================================================

BUILDROOT=/var/build/opticon-api_$VERSION

[ -d $BUILDROOT ] && rm -rf $BUILDROOT

mkdir -p $BUILDROOT || exitfail Could not create build dir
mkdir -p $BUILDROOT/etc/opticon
mkdir -p $BUILDROOT/var/opticon/db
mkdir -p $BUILDROOT/lib/systemd/system
mkdir -p $BUILDROOT/usr/sbin
mkdir -p $BUILDROOT/DEBIAN

# Create debian control file
cat > $BUILDROOT/DEBIAN/control << _EOF_
Package: opticon-api
Version: $VERSION
Section: base
Priority: optional
Requires: libmicrohttpd12
Architecture: amd64
Maintainer: NewVM <info@newvm.com>
Description: Opticon API Server
 Provides API access to the opticon database.
_EOF_

# Create debian post-install script
cp pkg/opticon-api.debian-postinst.sh $BUILDROOT/DEBIAN/postinst

# Copy binaries, scripts and example config
cp bin/opticon-api $BUILDROOT/usr/sbin/
chmod 750 $BUILDROOT/usr/sbin/opticon-api
cp init/opticon-api.service $BUILDROOT/lib/systemd/system/
cp src/opticon-api/opticon-api.conf.example $BUILDROOT/etc/opticon/

# Build the package
dpkg-deb --build $BUILDROOT || exitfail Could not build
rm -rf $BUILDROOT
mkdir -p pkg/deb
cp ${BUILDROOT}.deb pkg/deb/

# =============================================================================
# CLI build
# =============================================================================

BUILDROOT=/var/build/opticon-cli_$VERSION

[ -d $BUILDROOT ] && rm -rf $BUILDROOT

mkdir -p $BUILDROOT || exitfail Could not create build dir
mkdir -p $BUILDROOT/etc/opticon
mkdir -p $BUILDROOT/usr/bin
mkdir -p $BUILDROOT/DEBIAN

# Create debian control file
cat > $BUILDROOT/DEBIAN/control << _EOF_
Package: opticon-cli
Version: $VERSION
Section: base
Priority: optional
Requires: libcurl4
Architecture: amd64
Maintainer: NewVM <info@newvm.com>
Description: Opticon CLI client
 Provides CLI access to the opticon API.
_EOF_

# Create debian post-install script
cp pkg/opticon-cli.debian-postinst.sh $BUILDROOT/DEBIAN/postinst

# Copy binaries, scripts and example config
cp bin/opticon $BUILDROOT/usr/bin/
chmod 755 $BUILDROOT/usr/bin/opticon
cp src/opticon-cli/opticon-cli.conf.example $BUILDROOT/etc/opticon/

# Build the package
dpkg-deb --build $BUILDROOT || exitfail Could not build
rm -rf $BUILDROOT
mkdir -p pkg/deb
cp ${BUILDROOT}.deb pkg/deb/

