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

BUILDROOT=/var/build/opticon-agent-$VERSION

[ -d $BUILDROOT ] && rm -rf $BUILDROOT

mkdir -p $BUILDROOT || exitfail Could not create build dir
mkdir -p $BUILDROOT/config/scripts/post-config.d
cp bin/opticon-agent $BUILDROOT/config/scripts/
cp bin/opticon-helper $BUILDROOT/config/scripts/
strip $BUILDROOT/config/scripts/opticon-agent
strip $BUILDROOT/config/scripts/opticon-helper
cp init/opticon-agent.init $BUILDROOT/config/scripts/
cp src/opticon-agent/opticon-agent.conf.example $BUILDROOT/config/
cp src/opticon-agent/opticon-defaultprobes.conf $BUILDROOT/config/
cp src.opticon-agent/helpers.conf $BUILDROOT/config/opticon-helpers.conf
cat > $BUILDROOT/config/scripts/post-config.d/opticon-agent << _EOF_
#!/bin/sh
/config/scripts/opticon-agent.init start
_EOF_
chmod 755 $BUILDROOT/config/scripts/post-config.d/opticon-agent

CWD="$(pwd)"
cd $BUILDROOT
tar zcf ${BUILDROOT}.edgeos.tar.gz .
cd "$CWD"

mkdir -p pkg/edgeos
cp ${BUILDROOT}.edgeos.tar.gz pkg/edgeos/
