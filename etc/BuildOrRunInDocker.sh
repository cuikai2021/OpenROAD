#!/bin/bash

# e.g.
# build_cmd="./etc/Build.sh -cmake=-DCMAKE_BUILD_TYPE=Debug -threads=$(($(nproc) / 2))";
# regression_cmd="./test/regression gpl"

cmd=$@
echo $cmd;

workdir=`pwd`
uid=`id -u`
gid=`id -g`
docker run -it --rm --privileged --network=host \
  --env="DISPLAY" --mount type=bind,src=$HOME/.Xauthority,dst=/home/dockeruser/.Xauthority --mount type=bind,src=/tmp/.X11-unix,dst=/tmp/.X11-unix \
  --user $uid:$gid --mount type=bind,src=/run/user/$uid/,dst=/run/user/$uid/ --env SSH_AUTH_SOCK=/run/user/$uid/keyring/ssh \
  --mount type=bind,src=$HOME/.ssh,dst=/home/dockeruser/.ssh \
  --mount type=bind,src=$HOME/.ssh,dst=/root/.ssh \
  --mount type=bind,src=$workdir,dst=$workdir \
  --entrypoint /bin/bash \
  registry.cn-shanghai.aliyuncs.com/icb/eden_dev:20230418T0353Z_8aa01a3 \
  -c "cd $workdir; $cmd"
