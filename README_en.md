# xsocks

[![Build Status](https://travis-ci.com/Kinping-Tse/xsocks.svg?branch=master)](https://travis-ci.com/Kinping-Tse/xsocks)

## Intro [中文][readme_zh]

xsocks is a lightweight proxy tool for science online. Just for academic research, it is created and maintained by [@XJP09_HK][XJP] base on [shadowsocks-libev][].

## Feature

* Linux, OS X system support
* Encryption method is fully compatible with the shadowsocks protocol
* Usage is same as shadowsocks-libev mostly
* Simple builds, only depends on common source build tools
* The architecture is easy to support other protocols
* Excellent performance, can be compared using build-in benchmark tools

## Installation

```sh
# You must install these tools first: make (gcc,g++)|clang autoconf automake libtool git
$ git clone https://github.com/Kinping-Tse/xsocks.git
$ cd xsocks
$ make
$ make install
```
### Custom build

* Change install path

```sh
$ make PREFIX="Your_Path"
```
* Change memory allocator (default libc), only support jemalloc, libc now

```sh
$ make USE_JEMALLOC=yes
```
* Change network event model (default libev), only support libev, ae now

```sh
$ make USE_LIBEV=yes # Change libev
$ make USE_AE=yes # Change redis ae
```
* Shared library build (default static)

```sh
$ make USE_SHARED=yes
```
* Strip install

```sh
$ make install USE_STRIP=yes
```
* Cross build

```sh
$ make HOST="DEST_ARCH"
```
* Ubuntu build

```sh
$ make ubuntu
$ make && make install
$ exit
# Copy all the file in tmp directory to your ubuntu host
```
* ASUS router build (only support asuswrt-merlin.ng device now), [AM_TOOLCHAINS][]

```sh
# Make sure ubuntu build image is done, if not you can hit `make ubuntu` first
$ make asuswrt-merlin.ng AM_TOOLCHAINS_PATH="Your_AM_TOOLCHAINS_PATH"
# Copy all the file in tmp directory to your router
```
* etc

```sh
$ make OPTIMIZATION=-O3 # Change optimization level
$ make DEBUG=  # No need debug info
$ make V=1 # Visualization build
$ make clean # Clean
$ make distclean # Clean deeply, also include dependcy packages
```
### Build for developer

* Memory check

```sh
$ make valgrind
```
* Code coverage check

```sh
$ make gcov
$ builds/src/xs-local  # Run the test program
$ make lcov
$ # Open builds/src/lcov-html/index.html with browser
```
* Docker

```sh
$ make docker
```
## Usage

* Proxy tool usage

```
$ xs-[local|server|redir|tunnel] [options]

Options:
  [-s <server_host>]         Host name or IP address of your remote server (default 127.0.0.1)
  [-p <server_port>]         Port number of your remote server (default 8388)
  [-l <local_port>]          Port number of your local server (default 1080)
  [-k <password>]            Password of your remote server (default foobar)
  [-m <encrypt_method>]      Encrypt method: rc4-md5,
                             aes-128-gcm, aes-192-gcm, aes-256-gcm,
                             aes-128-cfb, aes-192-cfb, aes-256-cfb,
                             aes-128-ctr, aes-192-ctr, aes-256-ctr,
                             camellia-128-cfb, camellia-192-cfb, camellia-256-cfb,
                             bf-cfb, chacha20-ietf-poly1305,
                             xchacha20-ietf-poly1305,
                             salsa20, chacha20 and chacha20-ietf
                             (default aes-256-cfb)
  [-L <addr>:<port>]         Destination server address and port
                             for local port forwarding, only for xs-tunnel (default 8.8.8.8:53)
  [-f <pid_file>]            The file path to store pid
  [-t <timeout>]             Socket timeout in seconds (default 60)
  [-c <config_file>]         The path to config file
  [-b <local_address>]       Local address to bind
  [-u]                       Enable UDP relay
  [-U]                       Enable UDP relay and disable TCP relay
  [-6]                       Use IPv6 address first
  [--key <key_in_base64>]    Key of your remote server
  [--logfile <file>]         Log file
  [--loglevel <level>]       Log level (default info)
  [-v]                       Verbose mode
  [-V, --version]            Print version info
  [-h, --help]               Print this message
```
* Benchmark usage

```sh
$ make bench
$ ./builds/src/xs-benchmark-server
$ ./builds/src/xs-server
$ ./builds/src/xs-benchmark-client
```
* Docker

```sh
$ docker pull alucard5867/xsocks
$ docker-compose -f docker/docker-compose.yml up -d
```

## Architecture of source code

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

[readme_zh]: https://github.com/Kinping-Tse/xsocks/blob/master/README_zh.md
[XJP]: https://github.com/Kinping-Tse "XJP09_HK"
[shadowsocks-libev]: https://github.com/shadowsocks/shadowsocks-libev "shadowsocks-libev"
[AM_TOOLCHAINS]: https://github.com/RMerl/am-toolchains
