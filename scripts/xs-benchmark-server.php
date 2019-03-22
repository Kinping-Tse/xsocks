#!/usr/bin/env php
<?php
/*
 * This file is part of xsocks, a lightweight proxy tool for science online.
 *
 * Copyright (C) 2019 XJP09_HK <jianping_xie@aliyun.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

class Server {

    public $client_addrinfo_list = [];
    public $debug = 1;
    public $server;

    public function onReceive($server, $fd, $reactor_id, $data) {
        if ($this->debug) {
            if (empty($this->client_addrinfo_list[$fd])) {
                $client_info = $server->getClientInfo($fd);
                printf("TCP server accepted client %s\n", getAddrinfo($client_info));
                $this->client_addrinfo_list[$fd] = $client_info;
            }
        }

        $server->send($fd, $data);
    }

    public function onClose($server, $fd) {
        if ($this->debug && !empty($this->client_addrinfo_list[$fd])) {
            printf("TCP client %s closed connection\n", getAddrinfo($this->client_addrinfo_list[$fd]));
            unset($this->client_addrinfo_list[$fd]);
        }
    }
}

$host = $argv[1] ?? '127.0.0.1';
$port = intval($argv[2] ?? 19999);
$debug = intval($argv[3] ?? 1);

$app = new Server();
$app->debug = $debug;

function getAddrinfo($client_info) {
    return $client_info['remote_ip'] . ':' . $client_info['remote_port'] ;
}

printf("TCP server listen at %s:%d\n", $host, $port);

$app->server = new Swoole\Server($host, $port);
$app->server->on('receive', [$app, 'onReceive']);
$app->server->on('close', [$app, 'onClose']);
$app->server->start();
