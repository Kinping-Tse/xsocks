//
// This file is part of xsocks, a lightweight proxy tool for science online.
//
// Copyright (C) 2019 XJP09_HK <jianping_xie@aliyun.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

package main

import (
	"flag"
	"fmt"
	"io"
	"net"
	"strconv"
	"sync"
	"time"
)

var clientCount int

func handleConnection(conn net.Conn, debug bool) {
	defer conn.Close()

	conn.SetReadDeadline(time.Now().Add(2 * time.Minute))
	rbuf := make([]byte, 1024*16)
	var mux sync.Mutex

	if debug {
		fmt.Printf("TCP server accepted client %s\n", conn.RemoteAddr())
		mux.Lock()
		clientCount++
		fmt.Printf("TCP current client count: %d\n", clientCount)
		mux.Unlock()
	}

	for {
		nread, err := conn.Read(rbuf)
		if err != nil && debug {
			if err == io.EOF {
				fmt.Printf("TCP client %s closed connection\n", conn.RemoteAddr())
			} else {
				fmt.Printf("TCP read error: %s\n", err)
			}
			break
		}

		_, err = conn.Write(rbuf[:nread])
		if err != nil && debug {
			fmt.Println("TCP write error: %s\n", err)
			break
		}
	}

	if debug {
		mux.Lock()
		clientCount--
		fmt.Printf("TCP current client count: %d\n", clientCount)
		mux.Unlock()
	}
}

func main() {
	host := flag.String("host", "127.0.0.1", "Server hostname")
	port := flag.String("port", "19999", "Server port")
	debug := flag.String("debug", "1", "Dump debug info")

	flag.Parse()

	tcpAddr, _ := net.ResolveTCPAddr("tcp", *host+":"+*port)
	listener, err := net.ListenTCP("tcp", tcpAddr)
	if err != nil {
		fmt.Println(err)
		return
	}

	fmt.Printf("TCP server listen at %s\n", tcpAddr.String())

	for {
		conn, err := listener.Accept()
		if err != nil {
			fmt.Println(err)
			continue
		}

		isdebug, _ := strconv.ParseBool(*debug)
		go handleConnection(conn, isdebug)
	}
}
