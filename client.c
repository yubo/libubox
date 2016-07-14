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
#include "jsonrpc.h"

#define ADDR "127.0.0.1:1105"	// the port users will be connecting to

struct jrpc_client my_client;

int main(void)
{
	char *str_reply;
	struct json *reply, *item1, *item2, *params;
	struct jrpc_client *client = &my_client;

	int ret = jrpc_client_init(client, ADDR);
	if (ret != 0) {
		exit(ret);
	}

	item1 = json_create_object();
	json_add_number_to_object(item1, "A", 3);
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

	str_reply = json_sprint(reply);
	printf("%s\n", str_reply);

	free(str_reply);
	json_delete(reply);
	jrpc_client_close(client);

	return 0;
}
