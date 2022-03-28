#!/bin/bash

# the dependency docker to build or run in docker
deps_docker=registry.cn-shanghai.aliyuncs.com/ieden/eden_deps_ubuntu:220328or-tools

# e.g.
# build_cmd="./etc/Build.sh -cmake=-DCMAKE_BUILD_TYPE=Debug -threads=$(($(nproc) / 2))";
# regression_cmd="./test/regression gpl"

cmd=$@
echo $cmd;

workdir=`pwd`
docker run -it --rm --network=host \
  --mount type=bind,src=$workdir,dst=$workdir \
  --user `id -u`:`id -g` \
  --entrypoint /bin/bash \
  $deps_docker \
  -c "cd $workdir; $cmd"
