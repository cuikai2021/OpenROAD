#! /bin/sh
docker run --rm --network=host \
  --workdir /Eden \
  --entrypoint /bin/bash \
  --mount type=bind,source=$1,target=/Eden/devops_results \
  registry.cn-shanghai.aliyuncs.com/openroad/regression:0514 \
  -c "./build/src/utEden --gtest_output=json:gtest_output.json ; sh /Eden/etc/CodeCoverage ; sh /Eden/test/regression_update_failures"
 
