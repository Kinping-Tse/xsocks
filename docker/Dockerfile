FROM alpine:3.6

MAINTAINER XJP09_HK <jianping_xie@aliyun.com>

ENV XSOCKS_PATH /usr/local/xsocks

RUN set -xe \
    && apk add --no-cache --virtual .build-deps \
        make \
        gcc \
        g++ \
        autoconf \
        automake \
        libtool \
        linux-headers \
        git \
    && TMP_XSOCKS=/tmp/xsocks \
    && ( \
        git clone https://github.com/Kinping-Tse/xsocks.git $TMP_XSOCKS \
        && cd $TMP_XSOCKS \
        && make -j "$(getconf _NPROCESSORS_ONLN)" \
            PREFIX="$XSOCKS_PATH" \
            OPTIMIZATION=-O3 DEBUG= \
            USE_JEMALLOC=yes USE_LIBEV=yes USE_SHARED=no \
        && make install USE_STRIP=yes \
        && make distclean \
    ) \
    && runDeps="$( \
        scanelf --needed --nobanner --recursive $XSOCKS_PATH \
            | awk '{ gsub(/,/, "\nso:", $2); print "so:" $2 }' \
            | sort -u \
            | xargs -r apk info --installed \
            | sort -u \
    )" \
    && apk add --no-cache --virtual .run-deps $runDeps \
    && apk del .build-deps \
    && rm -rf $TMP_XSOCKS

WORKDIR $XSOCKS_PATH

CMD ["bin/xs-server", "-c", "etc/config.json"]
