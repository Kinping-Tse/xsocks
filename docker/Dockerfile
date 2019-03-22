#
# This file is part of xsocks, a lightweight proxy tool for science online.
#
# Copyright (C) 2019 XJP09_HK <jianping_xie@aliyun.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.
#

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
            USE_JEMALLOC=yes USE_LIBEV=yes USE_SHARED=yes \
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
