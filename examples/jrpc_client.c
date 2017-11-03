/*
 * Copyright 2016 Xiaomi Corporation. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 *
 * Authors:    Yu Bo <yubo@xiaomi.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "libubox/jsonrpc.h"

//#define HOST "127.0.0.1"	// host addr
#define HOST "unix:/tmp/test.sock"	// host addr
#define PORT "1234"	        // the port

struct jrpc_client my_client;

int main(void)
{
	struct json *reply, *item1, *item2, *params;
	struct jrpc_client *client = &my_client;

	json_init_hooks(NULL);

	int ret = jrpc_client_init(client, HOST, PORT);
	if (ret != 0) {
		exit(ret);
	}

	item1 = json_create_object();
	json_add_number_to_object(item1, "A", 15000);
	json_add_number_to_object(item1, "B", 10);

	item2 = json_create_object();
	json_add_number_to_object(item2, "A", 1);
	json_add_number_to_object(item2, "B", 2);

	params = json_create_array();
	json_add_item_to_object(item1,"S", item2);
	json_add_item_to_array(params, item1);

	// jrpc_client_call will free params
	if ((ret = jrpc_client_call(client, "foo", params, &reply)) != 0) {
		exit(ret);
	}

	printf("%s\n", json_to_string(reply));

	json_delete(reply);
	jrpc_client_close(client);

	return 0;
}
