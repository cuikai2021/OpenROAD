#!/bin/bash

numThreads=$(($(nproc) / 2))

build_cmd="./etc/Build.sh -cmake=-DCMAKE_BUILD_TYPE=Debug -threads=$numThreads";
echo $build_cmd;

workdir=`pwd`
docker run -it --rm --network=host \
  --mount type=bind,src=$workdir,dst=$workdir \
  --user `id -u`:`id -g` \
  --entrypoint /bin/bash \
  registry.cn-shanghai.aliyuncs.com/ieden/eden_deps_ubuntu:220218or-tools \
  -c "cd $workdir; $build_cmd"
