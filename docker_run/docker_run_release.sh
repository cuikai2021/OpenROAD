#! /bin/sh
docker run --rm --network=host \
  --workdir /Eden \
  --entrypoint /bin/bash \
  $1