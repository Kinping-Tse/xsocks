# xsocks

[![Build Status](https://travis-ci.com/Kinping-Tse/xsocks.svg?branch=master)](https://travis-ci.com/Kinping-Tse/xsocks)

## 简介 [English](https://github.com/Kinping-Tse/xsocks/blob/master/README.md)

xsocks 是一个轻量级的科学上网代理工具。

目前是由 [@XJP09_HK](https://github.com/Kinping-Tse) 创建并更新维护的。

## 特点

* 目前仅支持Linux, Mac OSX系统
* 加密方式完全兼容shadowsocks协议, 可与此协议相关工具配合或者独立使用
* 使用方法与shadowsocks-libev大部分雷同
* 架构合理, 很容易做到其他协议的支持
* 性能突出, 内置压测工具

## 安装方法

```sh
git clone https://github.com/Kinping-Tse/xsocks.git
cd xsocks
make
make install
```

### 定制化安装

* 自定义安装目录
```
make PREFIX="你的安装目录"
```
* 选择内存分配器(默认libc), 目前仅支持jemalloc, libc
```
make USE_JEMALLOC=yes
```
* 选择网络事件框架(默认libev), 目前仅支持libev, ae
```
make USE_LIBEV=yes
make USE_AE=yes
```
* 使用动态链接库安装(默认静态库方式)
```
make USE_SHARED=yes
```
* 压缩安装
```
make install USE_STRIP=yes
```
* docker
```
make docker
```
* 华硕路由器安装方式 (仅支持asuswrt-merlin.ng的设备)
```
make asuswrt-merlin.ng # 然后把tmp目录下的文件拷贝到你的路由器当中即可运行
```

### 供开发者使用的安装

* 压测
```
make bench
```
* 内存检测
```
make valgrind
```
* 代码覆盖检测

```
make gcov
make lcov
```

## 使用方法

* 所有工具均可使用--help选项查看
* 压测使用
```
./builds/src/xs-benchmark-server
./builds/src/xs-server
./builds/src/xs-benchmark-client
```
* docker, 当然你得先make docker生成docker镜像
```
cd docker
docker-compose -f docker-compose.yml up -d
```
