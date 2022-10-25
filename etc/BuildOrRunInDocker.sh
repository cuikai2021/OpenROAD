#!/bin/bash

EDEN_DIR=$(realpath $(dirname $0)/..)
# the dependency docker to build or run in docker
build_docker_tag=$(find $EDEN_DIR -name "*.so" | xargs cat | sha1sum | cut -c 1-7)
build_docker=registry.cn-shanghai.aliyuncs.com/icb/eden_build_ubuntu:${build_docker_tag}
if ! (docker inspect $build_docker  || docker manifest inspect $build_docker )>/dev/null 2>&1; then
    echo "build deps docker"
    python3 $EDEN_DIR/docker/build.py build-deps
fi

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
