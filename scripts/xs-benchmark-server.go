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
