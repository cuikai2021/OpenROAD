#!/bin/bash

set -euo pipefail

cd "$(dirname $(readlink -f $0))/../"

find . -name '*.so' -exec cp "{}" /usr/local/lib/ \;

find . -name 'maxhs' -exec cp "{}" /usr/local/bin/ \;