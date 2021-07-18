#! /bin/sh
docker run --rm --network=host \
  --workdir /Eden \
  --entrypoint /bin/bash \
  --mount type=bind,source=$1,target=/Eden/devops_results \
  --mount type=bind,source=$2,target=/Eden/coverage_results \
  $3 \
  -c "./build/src/utEden --gtest_output=json:gtest_output.json ; ./etc/CodeCoverage.sh ; sh /Eden/test/regression_update_failures"