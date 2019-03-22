#!/usr/bin/env node
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

const net = require('net');

const server = net.createServer((c) => {
    var addr = c.remoteAddress + ':' + c.remotePort;
    debug && console.log('TCP server accepted client ' + addr);

    c.on('close', (c) => {
        debug && console.log('TCP client ' + addr + ' closed connection');
    }).on('error', (err) => {
        console.log(err);
    }).pipe(c);
});

var host = '127.0.0.1';
var port = 19999;
var debug = 1;

process.argv.forEach((val, index) => {
    switch (index) {
        case 2: host = val; break;
        case 3: port = parseInt(val); break;
        case 4: debug = parseInt(val); break;
        default: break;
    }
});

server.listen({
    host: host,
    port: port,
    backlog: 256,
}, () => {
    console.log('TCP server listen at: ', server.address());
}).on('error', (err) => {
    console.log(err);
});
