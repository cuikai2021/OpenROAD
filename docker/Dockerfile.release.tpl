ARG EDEN_BUILD_IMAGE

# FROM
FROM ${EDEN_BUILD_IMAGE} AS release

# COPY
COPY tools_metadata.yaml /build/tools_metadata.yaml

ENV LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/build/lib/
ENV PATH=$PATH:/build/bin/

RUN apt-get update && apt-get install -y time /build/deb/*.deb && rm /build/deb -rf && apt-get clean
RUN mkdir -p /Eden-flow/flow && chmod o+rw -R /Eden-flow/flow
RUN touch ~/.tchlsh-history
WORKDIR /Eden-flow

FROM release AS debug
RUN apt-get update && apt-get install -y curl libgmp-dev && apt-get clean
RUN curl -o /tmp/gdb-12.1.tar.gz -L https://ftp.gnu.org/gnu/gdb/gdb-12.1.tar.gz && \
    tar -zxf /tmp/gdb-12.1.tar.gz -C /tmp && \
    cd /tmp/gdb-12.1 && \
    ./configure && \
    make -j8 && \
    make install && \
    rm -rf tmp/*
