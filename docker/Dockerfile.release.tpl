ARG EDEN_BUILD_IMAGE

# FROM
FROM ${EDEN_BUILD_IMAGE}

# COPY
COPY tools_metadata.yaml /build/tools_metadata.yaml

ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/build/lib/
ENV PATH=$PATH:/build/bin/

RUN apt-get update && \
    apt-get install -y time libffi7 /build/deb/*.deb && \
    rm /build/deb -rf && \
    apt-get clean && \
    mkdir -p /Eden-flow/flow && \
    chmod o+rw -R /Eden-flow/flow && \
    touch ~/.tchlsh-history

WORKDIR /Eden-flow
