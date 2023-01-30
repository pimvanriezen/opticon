#!/bin/sh
GITTAG=$(git describe --tags)
if [ ! $? = 0 ]; then
  VERSION=$(pwd | sed -e 's@/opticon-agent$@/@;s/.*opticon-agent-//;s/.*opticon-//;s@/.*@@;s/-0$//')
else
  VERSION=$(echo "$GITTAG" | cut -f1 -d' ' | cut -f1-2 -d-)
fi
cat > version.c << _EOF_
const char *VERSION = "${VERSION}";
_EOF_
