docker build \
-f docker/Dockerfile.regression \
-t registry.cn-shanghai.aliyuncs.com/openroad/regression:0514 \
--build-arg FROM_IMAGE_TAG=$1 \
