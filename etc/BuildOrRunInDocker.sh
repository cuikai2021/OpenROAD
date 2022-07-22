#!/bin/bash

# the dependency docker to build or run in docker
build_docker_tag=$(find . -name "*.so" | xargs sha1sum | cut -d ' ' -f 1 | sort | sha1sum)
build_docker=registry.cn-shanghai.aliyuncs.com/icb/eden_build_ubuntu:${build_docker_tag:0:7}

# e.g.
# build_cmd="./etc/Build.sh -cmake=-DCMAKE_BUILD_TYPE=Debug -threads=$(($(nproc) / 2))";
# regression_cmd="./test/regression gpl"

cmd=$@
echo $cmd;

workdir=`pwd`
docker run -it --rm --network=host \
  --env="DISPLAY" --volume="$HOME/.Xauthority:/root/.Xauthority:rw" \
  --mount type=bind,src=$workdir,dst=$workdir \
  --user `id -u`:`id -g` \
  --entrypoint /bin/bash \
  $build_docker \
  -c "cd $workdir; $cmd"
