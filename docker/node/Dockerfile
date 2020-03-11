FROM nanocurrency/nano-env:gcc

ARG NETWORK=live
ARG TRAVIS_TAG=DEV_BUILD
ARG CI_BUILD=OFF
ADD ./ /tmp/src

RUN mkdir /tmp/build && \
    cd /tmp/build && \
    cmake /tmp/src -DCI_BUILD=${CI_BUILD} -DBOOST_ROOT=${BOOST_ROOT} -DACTIVE_NETWORK=btcnew_${NETWORK}_network \
    -DBTCNEW_ROCKSDB=ON -DBTCNEW_POW_SERVER=ON -DROCKSDB_LIBRARIES=/tmp/rocksdb/lib/librocksdb.a \
    -DROCKSDB_INCLUDE_DIRS=/tmp/rocksdb/include && \
    make btcnew_node -j $(nproc) && \
    make btcnew_rpc -j $(nproc) && \
    make btcnew_pow_server -j $(nproc) && \
    cd .. && \
    echo ${NETWORK} > /etc/nano-network

FROM ubuntu:16.04

RUN groupadd --gid 1000 btcnew && \
    useradd --uid 1000 --gid btcnew --shell /bin/bash --create-home btcnew

COPY --from=0 /tmp/build/btcnew_node /usr/bin
COPY --from=0 /tmp/build/btcnew_rpc /usr/bin
COPY --from=0 /tmp/build/btcnew_pow_server /usr/bin
COPY --from=0 /etc/nano-network /etc
COPY docker/node/entry.sh /usr/bin/entry.sh
COPY docker/node/config /usr/share/nano/config
RUN chmod +x /usr/bin/entry.sh
RUN ln -s /usr/bin/btcnew_node /usr/bin/rai_node

WORKDIR /root
USER root

ENV PATH="${PATH}:/usr/bin"
ENTRYPOINT ["/bin/bash",  "/usr/bin/entry.sh"]
CMD ["btcnew_node daemon -l"]
