# xsocks

[![Build Status](https://travis-ci.com/Kinping-Tse/xsocks.svg?branch=master)](https://travis-ci.com/Kinping-Tse/xsocks)

## 简介 [English][readme_en]

**xsocks** 是一个轻量级的科学上网代理工具。仅是为了学术研究, 在参考[shadowsocks-libev][]代码的基础上由 [@XJP09_HK][XJP] 创建并更新维护的。

## 特点

* 目前仅支持Linux, Mac OSX系统
* 加密方式完全兼容shadowsocks协议, 可与此协议相关工具配合或者独立使用
* 使用方法与shadowsocks-libev大部分雷同
* 安装简易, 仅依赖常用的源码安装工具
* 架构合理, 很容易做到其他协议的支持
* 性能突出, 可使用内置压测工具进行对比

## 安装方法

```sh
# 需要预先安装以下工具: make (gcc,g++)|clang autoconf automake libtool git pkg-config
$ git clone https://github.com/Kinping-Tse/xsocks.git
$ cd xsocks
$ make
$ make install
```
### 定制化安装

* 自定义安装目录

```sh
$ make PREFIX="你的安装目录"
```
* 选择内存分配器(默认libc), 目前仅支持jemalloc, libc

```sh
$ make USE_JEMALLOC=yes
```
* 选择网络事件框架(默认libev), 目前仅支持libev, ae

```sh
$ make USE_LIBEV=yes # 选择libev
$ make USE_AE=yes # 选择redis ae
```
* 使用动态链接库安装(默认静态库方式)

```sh
$ make USE_SHARED=yes
```
* 压缩安装

```sh
$ make install USE_STRIP=yes
```
* 跨平台交叉编译

```sh
$ make HOST="目标平台"
```
* ubuntu环境编译

```sh
$ make ubuntu
$ make && make install
$ exit
# 把tmp目录的所有文件拷贝到你的ubuntu环境中, 即可运行
```
* 华硕路由器安装方式 (仅支持asuswrt-merlin.ng的设备), [华硕编译环境][AM_TOOLCHAINS]

```sh
$ make asuswrt-merlin.ng AM_TOOLCHAINS_PATH="你的华硕编译环境"
$ ./build.sh
$ exit
# 把tmp目录的所有文件拷贝到你的路由器当中, 即可运行
```
* 其他杂项

```sh
$ make OPTIMIZATION=-O3 # 优化等级
$ make DEBUG=  # 不需要编译调试选项
$ make V=1 # 编译可视化
$ make clean # 清理
$ make distclean # 深度清理, 依赖包也会被清理
```
### 供开发者使用的安装

* 内存检测

```sh
$ make valgrind
```
* 代码覆盖检测

```sh
$ make gcov
$ builds/src/xs-local  # 运行想要测试的程序
$ make lcov
$ # 通过浏览器打开 builds/src/lcov-html/index.html 即可查看
```
* docker

```sh
$ make docker
```
## 使用方法

* 代理工具使用

```
$ xs-[local|server|redir|tunnel] [选项]

选项:
  [-s <server_host>]         远端服务器的地址 (默认 127.0.0.1)
  [-p <server_port>]         远端服务器的端口号 (默认 8388)
  [-l <local_port>]          本地服务器的地址 (默认 1080)
  [-k <password>]            远端服务器使用密码 (默认 foobar)
  [-m <encrypt_method>]      加密方法: rc4-md5,
                             aes-128-gcm, aes-192-gcm, aes-256-gcm,
                             aes-128-cfb, aes-192-cfb, aes-256-cfb,
                             aes-128-ctr, aes-192-ctr, aes-256-ctr,
                             camellia-128-cfb, camellia-192-cfb, camellia-256-cfb,
                             bf-cfb, chacha20-ietf-poly1305,
                             xchacha20-ietf-poly1305,
                             salsa20, chacha20以及chacha20-ietf
                             (默认 aes-256-cfb)
  [-L <addr>:<port>]         本地服务器(仅xs-tunnel使用)端口代理转发的地址 (默认 8.8.8.8:53)
  [-f <pid_file>]            存储进程号文件路径, 开启后会自动进入守护后台模式
  [-t <timeout>]             socket超时时间, 单位秒 (默认 60)
  [-c <config_file>]         配置文件路径
  [-b <local_address>]       本地服务器的地址
  [-u]                       开启UDP代理模式
  [-U]                       开启UDP, 并同时关闭TCP
  [-6]                       优先使用ipv6地址
  [--acl <acl_file>]         ACL访问控制列表文件路径
  [--key <key_in_base64>]    远端服务器的Key
  [--logfile <file>]         日志文件
  [--loglevel <level>]       日志记录级别 (默认 info)
  [-v]                       日志级别为debug模式
  [-V, --version]            输出版本信息
  [-h, --help]               输出此帮助信息
```
* 压测使用

```sh
$ make bench
$ ./builds/src/xs-benchmark-server
$ ./builds/src/xs-server
$ ./builds/src/xs-benchmark-client
```
* docker部署

```sh
$ docker pull alucard5867/xsocks
$ docker-compose -f docker/docker-compose.yml up -d
```

## 代码架构

```
  +---------------------+
  |         app         |
  +----------------+    +
  |     module     |    |
  +-----+----------+----+
  |     |    protocol   |
  |     |---------------+
  | lib |     event     |
  |     |---------------+
  |     |     core      |
  +-----+---------------+
```

[readme_en]: https://github.com/Kinping-Tse/xsocks/blob/master/README_en.md
[XJP]: https://github.com/Kinping-Tse "XJP09_HK"
[shadowsocks-libev]: https://github.com/shadowsocks/shadowsocks-libev "shadowsocks-libev"
[AM_TOOLCHAINS]: https://github.com/RMerl/am-toolchains
