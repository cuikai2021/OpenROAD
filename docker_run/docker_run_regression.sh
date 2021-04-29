#! /bin/sh
docker run -it --rm --network=host \
  --workdir /Eden \
  --entrypoint /bin/bash \
  registry.cn-shanghai.aliyuncs.com/openroad/regression:lastest \
  -c "sh /Eden/test/regression || sh /Eden/test/regression_update_failures"
 
