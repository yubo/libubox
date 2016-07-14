/*
 * yubo@yubo.org
 * 2016-04-13
 */

package main

import (
	"fmt"
	"log"
	"net/rpc/jsonrpc"
)

const (
	URL = "127.0.0.1:1234"
)

type Args struct {
	A, B int
	S    struct {
		A, B int
	}
}

type Replay struct {
	Args []Args
	Str  string
}

func main() {
	args := Args{A: 3, B: 10}
	args.S.A = 1
	args.S.B = 2

	client, err := jsonrpc.Dial("tcp", URL)
	if err != nil {
		log.Fatal("dialing:", err)
	}

	{
		var reply string
		err = client.Call("sayHello", nil, &reply)
		if err != nil {
			log.Fatal("SayHello error:", err)
		}
		fmt.Printf("reply: %s\n", reply)
	}

	{
		reply := Replay{}
		err = client.Call("foo", args, &reply)
		if err != nil {
			fmt.Println(err)
		}
		fmt.Printf("reply: %v\n", reply)
	}

	//client.Close()
}
