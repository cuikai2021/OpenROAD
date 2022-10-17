#!/bin/bash

set -eo pipefail

cd "$(dirname $(readlink -f $0))/../"

if [ "$1" = "--docker" ]; then
  mkdir -p /build/usr/local/lib
  mkdir -p /build/usr/local/bin
  find . -name '*.so' -exec cp "{}" /build/usr/local/lib/ \;
else
  find . -name '*.so' -exec cp "{}" /usr/local/lib/ \;
fi

