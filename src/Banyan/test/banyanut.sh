#!/bin/bash
# icb fetch src/Banyan/test/cases/  -w ./in
pwd=`pwd`
proj_dir=`git rev-parse --show-toplevel`
rela_proj_dir=$(realpath "--relative-to=." "$proj_dir")
cd $rela_proj_dir
./etc/fetch.sh -f src/Banyan/test/cases
touch lang.json && echo "[]" > lang.json
$proj_dir/testa/runtests.py --lang lang.json ./build/src/Banyan/test/banyan_unittest
rm lang.json