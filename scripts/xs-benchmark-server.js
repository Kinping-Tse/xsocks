#!/usr/bin/env node

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
