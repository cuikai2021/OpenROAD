#! /bin/sh
docker run --rm --network=host \
  --workdir /Eden \
  --entrypoint /bin/bash \
  --mount type=bind,source="${JENKINS_HOME}"/devops_results,target=/Eden/devops_results \
  registry.cn-shanghai.aliyuncs.com/openroad/regression:0430 \
  -c "sh /Eden/test/regression || sh /Eden/test/regression_update_failures"
 
