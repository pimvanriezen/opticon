#!/bin/sh
VERSION=$(git describe --tags  2>/dev/null | cut -f1 -d' ' | cut -f1-2 -d-)
if [ -z "$VERSION" ]; then
  # we're in rpmbuild, hopefully
  VERSION=$(pwd | sed -e 's@/opticon-agent$@/@;s/.*opticon-agent-//;s/.*opticon-//;s@/.*@@')
  echo "-------------"
  echo "Version: $VERSION"
  echo "pwd: $(pwd)"
  echo "-------------"
fi
cat > version.c << _EOF_
const char *VERSION = "${VERSION}";
_EOF_
