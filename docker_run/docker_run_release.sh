#! /bin/sh
docker run --rm --network=host \
  --workdir /Eden \
  --entrypoint /bin/bash \
  --mount type=bind,source=$1,target=/Eden/build/src \
  $2