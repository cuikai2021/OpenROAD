#! /bin/sh
docker run --rm --network=host \
  --workdir /Eden \
  --entrypoint /bin/bash \
  --mount type=bind,source=$1,target=/Eden/devops_results \
  $2 \
  -c "./build/src/utEden --gtest_output=json:gtest_output.json ; ./etc/CodeCoverage.sh ; sh /Eden/test/regression_update_failures"