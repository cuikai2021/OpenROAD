#!/bin/bash

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
  registry.cn-shanghai.aliyuncs.com/icb/eden_build:latest \
  -c "cd $workdir; $cmd"
