# e.g.
# build_cmd="./etc/Build.sh -cmake=-DCMAKE_BUILD_TYPE=Debug -threads=$(($(nproc) / 2))";
# regression_cmd="./test/regression gpl"

cmd=$@
echo $cmd;

workdir=`pwd`
uid=`id -u`
gid=`id -g`
docker run -it --rm --privileged --network=host \
  --user $uid:$gid \
  --env="DISPLAY" \
  --mount type=bind,src=$HOME/.Xauthority,dst=/home/dockeruser/.Xauthority --mount type=bind,src=/tmp/.X11-unix,dst=/tmp/.X11-unix \
  --mount type=bind,src=/run/user/$uid/,dst=/run/user/$uid/ --env SSH_AUTH_SOCK=/run/user/$uid/keyring/ssh \
  --mount type=bind,src=$HOME/.ssh,dst=/root/.ssh \
  --mount type=bind,src=$workdir,dst=$workdir \
  --mount type=bind,src=$workdir/src/Banyan/banyan-integrate/banyan,dst=/build/bin/banyan \
  --mount type=bind,src=$workdir/src/Banyan/banyan-integrate/maxhs,dst=/build/bin/maxhs \
  --mount type=bind,src=$workdir/src/Banyan/banyan-integrate/sat_server,dst=/build/bin/sat_server \
  --mount type=bind,src=$workdir/src/Banyan/banyan-integrate/cgss2,dst=/build/bin/cgss2 \
  --mount type=bind,src=$workdir/src/Banyan/banyan-integrate/bypbdef,dst=/build/bin/bypbdef \
  --mount type=bind,src=$workdir/src/Banyan/banyan-integrate/minjog,dst=/build/bin/minjog \
  --mount type=bind,src=$workdir/src/Banyan/banyan-integrate/garble,dst=/build/bin/garble \
  --mount type=bind,src=$workdir/src/linden/gpl/bin/lindendp,dst=/build/bin/lindendp \
  --mount type=bind,src=$workdir/src/linden/gpl/bin/linden-incremental-legalizer,dst=/build/bin/linden-incremental-legalizer \
  --mount type=bind,src=$workdir/src/linden/gpl/bin/linden-routability-driven-detailed-placer,dst=/build/bin/linden-routability-driven-detailed-placer \
  --mount type=bind,src=$workdir/src/linden/gpl/bin/deblais-dp,dst=/build/bin/deblais-dp \
  --mount type=bind,src=$workdir/src/linden/gpl/bin/deblais-incrdp,dst=/build/bin/deblais-incrdp \
  --mount type=bind,src=$workdir/src/linden/gpl/bin/pbdef,dst=/build/bin/pbdef \
  --mount type=bind,src=$workdir/src/linden/gpl/bin/linden-add-tie-cell,dst=/build/bin/linden-add-tie-cell \
  --entrypoint /bin/bash \
  registry.cn-shanghai.aliyuncs.com/icb/eden_dev:20240709T0624Z_afb4115 \
  -c "cd $workdir; $cmd"
