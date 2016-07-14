/*
 * Copyright 2016 yubo. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
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

#include "ustream.h"
#include "uloop.h"
#include "usock.h"
#include "jsonrpc.h"
#include "ulog.h"

struct client *next_client = NULL;

static void jrpc_procedure_destroy(struct jrpc_procedure *procedure);


static int send_request(struct jrpc_connection *conn, char *request)
{
	int fd = conn->sock.fd;
	if (conn->debug_level > 1)
		dlog("JSON Request:\n%s\n", request);
	write(fd, request, strlen(request));
	write(fd, "\n", 1);
	return 0;
}


static int send_response(struct jrpc_connection *conn, char *response)
{
	int n;
	int fd = conn->sock.fd;
	if (conn->debug_level > 1)
		dlog("JSON Response:\n%s\n", response);
	n = write(fd, response, strlen(response));
	n += write(fd, "\n", 1);
	return n;
}

static int send_error(struct jrpc_connection *conn, int code, char *message,
		      struct json *id)
{
	int return_value = 0;
	struct json *result_root = json_create_object();
	struct json *error_root = json_create_object();
	json_add_number_to_object(error_root, "code", code);
	json_add_string_to_object(error_root, "message", message);
	json_add_item_to_object(result_root, "error", error_root);
	json_add_item_to_object(result_root, "id", id);
	return_value = send_response(conn,
			json_to_string_unformatted(result_root));
	json_delete(result_root);
	free(message);
	return return_value;
}

static int send_result(struct jrpc_connection *conn, struct json *result,
		       struct json *id)
{
	int return_value = 0;
	struct json *result_root = json_create_object();
	if (result)
		json_add_item_to_object(result_root, "result", result);
	json_add_item_to_object(result_root, "id", id);

	return_value = send_response(conn,
			json_to_string_unformatted(result_root));
	json_delete(result_root);
	return return_value;
}

static int invoke_procedure(struct jrpc_server *server,
			    struct jrpc_connection *conn, char *name,
			    struct json *params, struct json *id)
{
	struct json *returned = NULL;
	int procedure_found = 0;
	struct jrpc_context ctx;
	ctx.error_code = 0;
	ctx.error_message = NULL;
	int i = server->procedure_count;
	while (i--) {
		if (!strcmp(server->procedures[i].name, name)) {
			procedure_found = 1;
			ctx.data = server->procedures[i].data;
			returned =
			    server->procedures[i].function(&ctx, params, id);
			break;
		}
	}
	if (!procedure_found)
		return send_error(conn, JRPC_METHOD_NOT_FOUND,
				  strdup("Method not found."), id);
	else {
		if (ctx.error_code)
			return send_error(conn, ctx.error_code,
					  ctx.error_message, id);
		else
			return send_result(conn, returned, id);
	}
}

static int invoke_procedure_id(struct jrpc_server *server, struct json *method,
			       struct jrpc_connection *conn, struct json *id,
			       struct json *params)
{
	//We have to copy ID because using it on the reply and deleting the response Object will also delete ID
	struct json *id_copy = NULL;
	if (id != NULL)
		id_copy = (id->type == JSON_T_STRING)
		    ? json_create_string(id->valuestring)
		    : json_create_number(id->valueint);
	if (server->debug_level)
		dlog("Method Invoked: %s\n", method->valuestring);
	return invoke_procedure(server, conn, method->valuestring, params,
				id_copy);
}

static int eval_request(struct jrpc_server *server,
			struct jrpc_connection *conn, struct json *root)
{
	struct json *method, *params, *id;
	method = json_get_object_item(root, "method");

	if (method != NULL && method->type == JSON_T_STRING) {
		params = json_get_object_item(root, "params");
		if (params == NULL || params->type == JSON_T_ARRAY
		    || params->type == JSON_T_OBJECT) {
			id = json_get_object_item(root, "id");
			if (id == NULL || id->type == JSON_T_STRING
			    || id->type == JSON_T_NUMBER) {
				return invoke_procedure_id(server, method, conn,
							   id, params);
			}

		}
	}
	send_error(conn, JRPC_INVALID_REQUEST,
		   strdup("The JSON sent is not a valid Request object."),
		   NULL);
	return -1;
}

static void close_connection(struct uloop_fd *sock)
{
	struct jrpc_connection *conn;

	conn = container_of(sock, struct jrpc_connection, sock);
	uloop_fd_delete(sock);
	close(sock->fd);
	free(conn->buffer);
	free(conn);
}

static void connection_cb(struct uloop_fd *sock, unsigned int events)
{
	struct json *root;
	int fd, max_read_size;
	struct jrpc_connection *conn;
	struct jrpc_server *server;
	ssize_t bytes_read = 0;
	char *new_buffer, *end_ptr = NULL;

	conn = container_of(sock, struct jrpc_connection, sock);
	fd = sock->fd;
	server = conn->server;

	if (conn->pos == (conn->buffer_size - 1)) {
		conn->buffer_size *= 2;
		new_buffer = realloc(conn->buffer, conn->buffer_size);
		if (new_buffer == NULL) {
			elog("Memory error %s\n", strerror(errno));
			return close_connection(sock);
		}
		conn->buffer = new_buffer;
		memset(conn->buffer + conn->pos, 0,
		       conn->buffer_size - conn->pos);
	}
	// can not fill the entire buffer, string must be NULL terminated
	max_read_size = conn->buffer_size - conn->pos - 1;



	do {
		bytes_read = read(fd, conn->buffer + conn->pos, max_read_size);
		if (bytes_read < 0) {
			if (errno == EINVAL)
				continue;

			if (errno == EAGAIN)
				break;

			// error
			return close_connection(sock);
		}

		if (bytes_read == 0) {
			// client closed the sending half of the connection
			if (server->debug_level)
				dlog("Client closed connection.\n");
			return close_connection(sock);
		}
		break;
	} while (1);

	conn->pos += bytes_read;

	if ((root = json_parse_stream(conn->buffer, &end_ptr)) != NULL) {
		if (server->debug_level > 1) {
			dlog("Valid JSON Received:\n%s\n",
					json_to_string(root));
		}

		if (root->type == JSON_T_OBJECT) {
			eval_request(server, conn, root);
		}
		//shift processed request, discarding it
		memmove(conn->buffer, end_ptr, strlen(end_ptr) + 2);

		conn->pos = strlen(end_ptr);
		memset(conn->buffer + conn->pos, 0,
		       conn->buffer_size - conn->pos - 1);

		json_delete(root);
	} else {
		// did we parse the all buffer? If so, just wait for more.
		// else there was an error before the buffer's end
		if (end_ptr != (conn->buffer + conn->pos)) {
			if (server->debug_level) {
				dlog("INVALID JSON Received:\n---\n%s\n---\n",
				       conn->buffer);
			}
			send_error(conn, JRPC_PARSE_ERROR,
				   strdup("Parse error. Invalid JSON"
					  " was received by the server."),
				   NULL);
			return close_connection(sock);
		}
	}

}

static struct jrpc_connection *new_connection(int fd, uloop_fd_handler cb)
{
	struct jrpc_connection *conn;

	conn = calloc(1, sizeof(*conn));
	if (!conn)
		return NULL;

	conn->sock.fd = fd;
	conn->sock.cb = cb;

	conn->buffer_size = 1500;
	conn->buffer = calloc(1, 1500);
	conn->pos = 0;

	return conn;
}

static void system_fd_set_cloexec(int fd)
{
#ifdef FD_CLOEXEC
	fcntl(fd, F_SETFD, fcntl(fd, F_GETFD) | FD_CLOEXEC);
#endif
}

static bool get_next_connection(int server_fd, struct jrpc_server *server)
{
	struct jrpc_connection *conn;
	int fd;
	struct sockaddr_in a;
	socklen_t len;

	memset(&a, 0, sizeof(a));
	len = sizeof(struct sockaddr);
	fd = accept(server_fd, (struct sockaddr *)&a, &len);
	if (fd < 0) {
		/*
		   elog("accept[0x%08x] error[%d] error[%d] return false\n", server_fd, fd, errno);
		 */
		switch (errno) {
		case ECONNABORTED:
		case EINTR:
			return true;
		default:

			return false;
		}
	}
	conn = new_connection(fd, connection_cb);
	if (conn) {
		memcpy(&conn->addr, &a, sizeof(a));
		system_fd_set_cloexec(conn->sock.fd);
		conn->server = server;
		conn->debug_level = server->debug_level;
		uloop_fd_add(&conn->sock, ULOOP_READ);
	} else {
		close(fd);
	}

	return true;
}

static void server_cb(struct uloop_fd *sock, unsigned int events)
{
	bool next;
	struct jrpc_server *server =
	    container_of(sock, struct jrpc_server, sock);

	do {
		next = get_next_connection(sock->fd, server);
	} while (next);
}

int jrpc_server_init(struct jrpc_server *server, char *host, char *port)
{
	char *debug_level_env = getenv("JRPC_DEBUG");

	if (debug_level_env == NULL) {
		server->debug_level = 0;
	} else {
		server->debug_level = strtol(debug_level_env, NULL, 10);
		dlog("JSONRPC-C Debug level %d\n", server->debug_level);
	}

	server->sock.cb = server_cb;
	server->sock.fd =
	    usock(USOCK_TCP | USOCK_SERVER | USOCK_IPV4ONLY | USOCK_NUMERIC,
		  host, port);
	if (server->sock.fd < 0) {
		perror("usock");
		return 1;
	}

	system_fd_set_cloexec(server->sock.fd);
	uloop_fd_add(&server->sock, ULOOP_READ | ULOOP_EDGE_TRIGGER);
	return 0;
}

void jrpc_server_run(struct jrpc_server *server)
{
	uloop_fd_add(&server->sock, ULOOP_READ | ULOOP_EDGE_TRIGGER);
}

int jrpc_server_stop(struct jrpc_server *server)
{
	uloop_fd_delete(&server->sock);
	return 0;
}

void jrpc_server_destroy(struct jrpc_server *server)
{
	/* Don't destroy server */
	int i;
	for (i = 0; i < server->procedure_count; i++) {
		jrpc_procedure_destroy(&(server->procedures[i]));
	}
	free(server->procedures);
}

static void jrpc_procedure_destroy(struct jrpc_procedure *procedure)
{
	if (procedure->name) {
		free(procedure->name);
		procedure->name = NULL;
	}
	if (procedure->data) {
		free(procedure->data);
		procedure->data = NULL;
	}
}

struct client {
	struct sockaddr_in sin;

	struct ustream_fd s;
	int ctr;
};

int jrpc_register_procedure(struct jrpc_server *server,
			    jrpc_function function_pointer, char *name,
			    void *data)
{
	struct jrpc_procedure *ptr;
	int i = server->procedure_count++;

	if (!server->procedures)
		server->procedures = malloc(sizeof(struct jrpc_procedure));
	else {
		ptr =
		    realloc(server->procedures,
			    sizeof(struct jrpc_procedure) *
			    server->procedure_count);
		if (!ptr)
			return -1;
		server->procedures = ptr;

	}
	if ((server->procedures[i].name = strdup(name)) == NULL)
		return -1;
	server->procedures[i].function = function_pointer;
	server->procedures[i].data = data;
	return 0;
}

int jrpc_deregister_procedure(struct jrpc_server *server, char *name)
{
	/* Search the procedure to deregister */
	int i;
	struct jrpc_procedure *ptr;
	int found = 0;

	if (!server->procedures) {
		elog("server : procedure '%s' not found\n", name);
		return -1;
	}

	for (i = 0; i < server->procedure_count; i++) {
		if (found)
			server->procedures[i - 1] = server->procedures[i];
		else if (!strcmp(name, server->procedures[i].name)) {
			found = 1;
			jrpc_procedure_destroy(&(server->procedures[i]));
		}
	}

	if (!found)
		return 0;

	server->procedure_count--;

	if (!server->procedure_count) {
		server->procedures = NULL;
	}

	ptr = realloc(server->procedures, sizeof(struct jrpc_procedure)
		      * server->procedure_count);
	if (!ptr) {
		perror("realloc");
		return -1;
	}
	server->procedures = ptr;
	return 0;
}

/* jsonrpc client */
void jrpc_client_close(struct jrpc_client *client)
{
	close(client->conn.sock.fd);
	free(client->conn.buffer);
	client->conn.buffer = NULL;
}

int jrpc_client_init(struct jrpc_client *client, char *host, char *port)
{
	char *debug_level_env;

	memset(client, 0, sizeof(*client));
	debug_level_env = getenv("JRPC_DEBUG");
	if (debug_level_env == NULL)
		client->conn.debug_level = 0;
	else {
		client->conn.debug_level = strtol(debug_level_env, NULL, 10);
		dlog("JSONRPC-C Debug level %d\n", client->conn.debug_level);
	}

	strncpy(client->host, host, sizeof(client->host) - 1);
	strncpy(client->port, port, sizeof(client->host) - 1);
	client->conn.buffer_size = 1500;
	client->conn.buffer = calloc(1, 1500);
	client->conn.pos = 0;

	client->conn.sock.fd =
	    usock(USOCK_TCP | USOCK_IPV4ONLY | USOCK_NUMERIC,
		  host, port);

	if (client->conn.sock.fd < 0) {
		return 1;
	}

	return 0;
}

int jrpc_client_call(struct jrpc_client *client, const char *method,
		     struct json *params, struct json **response)
{
	int fd, max_read_size;
	size_t bytes_read = 0;
	char *new_buffer, *end_ptr = NULL;
	struct jrpc_connection *conn;
	struct json *root, *request;

	request = json_create_object();
	json_add_string_to_object(request, "method", method);
	json_add_item_to_object(request, "params", params);
	json_add_number_to_object(request, "id", client->id);

	send_request(&client->conn, json_to_string(request));
	json_delete(request);

	// read
	conn = &client->conn;
	fd = conn->sock.fd;

	for (;;) {
		if (conn->pos == (conn->buffer_size - 1)) {
			conn->buffer_size *= 2;
			new_buffer = realloc(conn->buffer, conn->buffer_size);
			if (new_buffer == NULL) {
				perror("Memory error");
				return -ENOMEM;
			}
			conn->buffer = new_buffer;
			memset(conn->buffer + conn->pos, 0,
			       conn->buffer_size - conn->pos);
		}
		// can not fill the entire buffer, string must be NULL terminated
		max_read_size = conn->buffer_size - conn->pos - 1;
		if ((bytes_read =
		     read(fd, conn->buffer + conn->pos, max_read_size))
		    == -1) {
			elog("read %d\n", strerror(errno));
			return -EIO;
		}
		if (!bytes_read) {
			// client closed the sending half of the connection
			if (client->conn.debug_level)
				dlog("Client closed connection.\n");
			return -EIO;
		}

		conn->pos += bytes_read;

		if ((root = json_parse_stream(conn->buffer, &end_ptr)) != NULL) {
			if (client->conn.debug_level > 1) {
				dlog("Valid JSON Received:\n%s\n",
				       json_to_string(root));
			}

			if (root->type == JSON_T_OBJECT) {
				struct json *id =
				    json_get_object_item(root, "id");

				if (id->type == JSON_T_STRING) {
					if (client->id != atoi(id->string))
						goto out;
				} else if (id->type == JSON_T_NUMBER) {
					if (client->id != id->valueint)
						goto out;
				}
				client->id++;
				//shift processed request, discarding it
				memmove(conn->buffer, end_ptr,
					strlen(end_ptr) + 2);
				conn->pos = strlen(end_ptr);
				memset(conn->buffer + conn->pos, 0,
				       conn->buffer_size - conn->pos - 1);

				*response =
				    json_detach_item_from_object(root,
								 "result");
				if (*response == NULL)
					goto out;

				json_delete(root);
				return 0;
			}
out:
			elog("INVALID JSON Received:\n---\n%s\n---\n",
			       conn->buffer);
			json_delete(root);
			return -EINVAL;
		} else if (end_ptr != (conn->buffer + conn->pos)) {
			// did we parse the all buffer? If so, just wait for more.
			// else there was an error before the buffer's end
			if (client->conn.debug_level) {
				elog("INVALID JSON Received:\n---\n%s\n---\n",
				       conn->buffer);
			}
			send_error(conn, JRPC_PARSE_ERROR,
				   strdup("Parse error. Invalid JSON"
					  " was received by the client."),
				   NULL);
			return -EINVAL;
		}
	}
}
