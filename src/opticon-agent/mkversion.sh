#!/bin/sh
VERSION=$(git describe --tags | cut -f1 -d' ' | cut -f1-2 -d-)
cat > version.c << _EOF_
const char *VERSION = "${VERSION}";
_EOF_
