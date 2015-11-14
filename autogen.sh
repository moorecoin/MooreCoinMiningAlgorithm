#!/bin/sh
set -e
srcdir="$(dirname $0)"
cd "$srcdir"
if [ -z ${libtoolize} ] && glibtoolize="`which glibtoolize 2>/dev/null`"; then
  libtoolize="${glibtoolize}"
  export libtoolize
fi
autoreconf --install --force --warnings=all
