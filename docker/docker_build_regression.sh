docker build \
-f docker/Dockerfile.regression \
-t registry.cn-shanghai.aliyuncs.com/openroad/regression:lastest \
--build-arg aki=LTAI5tA8PnQGSDgbJBuQhsg1 \
--build-arg  aks=jm1TitHY49vTJPncLDiZhhwEs6huHo .