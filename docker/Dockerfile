FROM alpine:3.6

MAINTAINER XJP09_HK <jianping_xie@aliyun.com>

ENV XSOCKS_PATH /usr/local/xsocks

COPY . /tmp/xsocks

RUN set -xe \
    && apk add --no-cache --virtual .build-deps \
        make \
        gcc \
        g++ \
        autoconf \
        automake \
        libtool \
        linux-headers \
    && ( \
        cd /tmp/xsocks \
        && make -j "$(getconf _NPROCESSORS_ONLN)" \
            PREFIX="$XSOCKS_PATH" \
            USE_JEMALLOC=no USE_LIBEV=no USE_SHARED=no \
        && make install \
        && make distclean \
    ) \
    && apk del .build-deps \
    && rm -rf /tmp/xsocks

WORKDIR $XSOCKS_PATH

CMD ["bin/xs-server", "-c", "etc/config.json"]
