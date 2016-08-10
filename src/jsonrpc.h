/*
 * Copyright 2016 yubo. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */ 

#ifndef __JSONRPC_H__
#define __JSONRPC_H__

#include "libubox/uloop.h"
#include "libubox/json.h"

/*
 *
 * http://www.jsonrpc.org/specification
 *
 * code	message	meaning
 * -32700	Parse error	Invalid JSON was received by the server.
 * An error occurred on the server while parsing the JSON text.
 * -32600	Invalid Request	The JSON sent is not a valid Request object.
 * -32601	Method not found	The method does not exist / is not available.
 * -32602	Invalid params	Invalid method parameter(s).
 * -32603	Internal error	Internal JSON-RPC error.
 * -32000 to -32099	Server error	Reserved for implementation-defined server-errors.
 */

#define JRPC_PARSE_ERROR -32700
#define JRPC_INVALID_REQUEST -32600
#define JRPC_METHOD_NOT_FOUND -32601
#define JRPC_INVALID_PARAMS -32603
#define JRPC_INTERNAL_ERROR -32693

struct jrpc_context{
	void *data;
	int error_code;
	char *error_message;
};

typedef struct json *(*jrpc_function) (struct jrpc_context * context, struct json * params,
				 struct json * id);

struct jrpc_procedure {
	char *name;
	jrpc_function function;
	void *data;
};

struct jrpc_server {
	char *host;
	char *port;
	struct uloop_fd sock;
	int procedure_count;
	struct jrpc_procedure *procedures;
	int debug_level;
};

struct jrpc_connection {
	struct uloop_fd sock;
	struct sockaddr_in addr;
	int pos;
	unsigned int buffer_size;
	char *buffer;
	int debug_level;
	struct jrpc_server *server;
};

int jrpc_server_init(struct jrpc_server *server, char *host, char *port);
/*void jrpc_server_run(struct jrpc_server *server);
int jrpc_server_stop(struct jrpc_server *server);*/
void jrpc_server_destroy(struct jrpc_server *server);
int jrpc_register_procedure(struct jrpc_server *server,
			    jrpc_function function_pointer, char *name,
			    void *data);
int jrpc_deregister_procedure(struct jrpc_server *server, char *name);

/* jsonrpc client */
struct jrpc_client {
	char host[64];
	char port[8];
	int id;
	struct jrpc_connection conn;
};

void jrpc_client_close(struct jrpc_client *client);
int jrpc_client_init(struct jrpc_client *client, char *host, char *port);
int jrpc_client_call(struct jrpc_client *client, const char *method,
		struct json *params, struct json **response);

#endif
