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
static void connection_write(struct jrpc_connection *conn, char *data, int len);

static int send_request(struct jrpc_connection *conn, char *request)
{
	if (conn->debug_level > 1)
		dlog("JSON Request:\n%s\n", request);
	connection_write(conn, request, strlen(request));
	connection_write(conn, "\n", 1);
	return 0;
}

static int send_response(struct jrpc_connection *conn, char *response)
{
	if (conn->debug_level > 1)
		dlog("JSON Response:\n%s\n", response);
	connection_write(conn, response, strlen(response));
	connection_write(conn, "\n", 1);
	return 0;
}

static int send_error(struct jrpc_connection *conn, int code, char *message,
		      struct json *id)
{
	struct json *result_root = json_create_object();
	struct json *error_root = json_create_object();
	json_add_number_to_object(error_root, "code", code);
	json_add_string_to_object(error_root, "message", message);
	json_add_item_to_object(result_root, "error", error_root);
	json_add_item_to_object(result_root, "id", id);
	send_response(conn, json_to_string_unformatted(result_root));
	json_delete(result_root);
	free(message);
	return 0;
}

static int send_result(struct jrpc_connection *conn, struct json *result,
		       struct json *id)
{
	struct json *result_root = json_create_object();
	if (result)
		json_add_item_to_object(result_root, "result", result);
	json_add_item_to_object(result_root, "id", id);

	send_response(conn, json_to_string_unformatted(result_root));
	json_delete(result_root);
	return 0;
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

static void close_connection(struct jrpc_connection *conn)
{
	uloop_fd_delete(&conn->sock);
	close(conn->sock.fd);
	free(conn->r.data);
	free(conn->w.data);
	free(conn);
}

static void connection_read_cb(struct jrpc_connection *conn)
{
	struct json *root;
	ssize_t bytes_read = 0;
	char *new_buffer, *end_ptr = NULL;
	struct buffer_t *buf = &conn->r;
	struct jrpc_server *server = conn->server;
	int max_read_size, fd = conn->sock.fd;

	if (buf->pos == (buf->size - 1)) {
		buf->size *= 2;
		new_buffer = realloc(buf->data, buf->size);
		if (new_buffer == NULL) {
			elog("Memory error %s\n", strerror(errno));
			return close_connection(conn);
		}
		buf->data = new_buffer;
	}
	// can not fill the entire buffer, string must be NULL terminated
	max_read_size = buf->size - buf->pos - 1;

	do {
		bytes_read = read(fd, buf->data + buf->pos, max_read_size);
		if (bytes_read < 0) {
			if (errno == EINVAL)
				continue;
			if (errno == EAGAIN)
				break;
			// error
			return close_connection(conn);
		}

		if (bytes_read == 0) {
			// client closed the sending half of the connection
			if (server->debug_level)
				dlog("Client closed connection.\n");
			return close_connection(conn);
		}
		break;
	} while (1);

	buf->pos += bytes_read;
	buf->data[buf->pos] = 0;

	if ((root = json_parse_stream(buf->data, &end_ptr)) != NULL) {
		if (server->debug_level > 1) {
			dlog("Valid JSON Received:\n%s\n",
			     json_to_string(root));
		}

		if (root->type == JSON_T_OBJECT) {
			eval_request(server, conn, root);
		}
		//shift processed request, discarding it
		buf->pos -= end_ptr - buf->data;
		memmove(buf->data, end_ptr, buf->pos);
		buf->data[buf->pos] = 0;

		json_delete(root);
	} else {
		// did we parse the all buffer? If so, just wait for more.
		// else there was an error before the buffer's end
		if (end_ptr != (buf->data + buf->pos)) {
			if (server->debug_level) {
				dlog("INVALID JSON Received:\n---\n%s\n---\n",
				     buf->data);
			}
			send_error(conn, JRPC_PARSE_ERROR,
				   strdup("Parse error. Invalid JSON"
					  " was received by the server."),
				   NULL);
			return close_connection(conn);
		}
	}

}

static void connection_write_cb(struct jrpc_connection *conn)
{
	struct buffer_t *buf = &conn->w;
	ssize_t bytes_write;
	int fd = conn->sock.fd;
	int size, offset;

	//tx buff first
	size = buf->pos;
	offset = 0;
	while (size > 0) {
		bytes_write = write(fd, buf->data + offset, size);
		if (bytes_write < 0) {
			if (errno == EINVAL)
				continue;
			if (errno == EAGAIN)
				break;
			// error
			return close_connection(conn);
		}
		offset += bytes_write;
		size = buf->pos - offset;
	}
	if (offset) {
		buf->pos -= offset;
		memmove(buf->data, buf->data+offset, buf->pos);
		buf->data[buf->pos] = 0;
	}
}


static void connection_write(struct jrpc_connection *conn, char *data, int len)
{
	char *new_buffer;
	struct buffer_t *buf = &conn->w;

	if (buf->pos + len > (buf->size -1)) {
		buf->size = len+1;
		new_buffer = realloc(buf->data, buf->size);
		if (new_buffer == NULL) {
			elog("Memory error %s\n", strerror(errno));
			return close_connection(conn);
		}
		buf->data = new_buffer;
	}

	memcpy(buf->data+buf->pos, data, len);
	buf->pos += len;

	connection_write_cb(conn);
}
static void connection_cb(struct uloop_fd *sock, unsigned int events)
{
	struct jrpc_connection *conn;
	conn = container_of(sock, struct jrpc_connection, sock);

	if (events & ULOOP_WRITE) {
		connection_write_cb(conn);
	}

	if (events & ULOOP_READ) {
		connection_read_cb(conn);
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

	conn->r.size = 1500;
	conn->r.data = calloc(1, 1500);
	conn->r.pos = 0;

	conn->w.size = 1500;
	conn->w.data = calloc(1, 1500);
	conn->w.pos = 0;

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
		uloop_fd_add(&conn->sock, ULOOP_READ | ULOOP_WRITE);
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
	int type = USOCK_TCP | USOCK_SERVER | USOCK_IPV4ONLY | USOCK_NUMERIC;

	if (debug_level_env == NULL) {
		server->debug_level = 0;
	} else {
		server->debug_level = strtol(debug_level_env, NULL, 10);
		dlog("JSONRPC-C Debug level %d\n", server->debug_level);
	}

	if (strncmp(host, "unix:", 5) == 0) {
		type |= USOCK_UNIX;
		host += 5;
	}

	server->sock.cb = server_cb;
	server->sock.fd = usock(type, host, port);
	if (server->sock.fd < 0) {
		perror("usock");
		return 1;
	}

	system_fd_set_cloexec(server->sock.fd);
	uloop_fd_add(&server->sock, ULOOP_READ | ULOOP_EDGE_TRIGGER);
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

/* jsonrpc client, single thread */
void jrpc_client_close(struct jrpc_client *client)
{
	close(client->conn.sock.fd);
	free(client->conn.r.data);
	free(client->conn.w.data);
}

int jrpc_client_init(struct jrpc_client *client, char *host, char *port)
{
	char *debug_level_env;
	int type = USOCK_TCP | USOCK_IPV4ONLY | USOCK_NUMERIC;

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
	client->conn.r.size = 1500;
	client->conn.r.data = calloc(1, 1500);
	client->conn.r.pos = 0;
	client->conn.w.size = 1500;
	client->conn.w.data = calloc(1, 1500);
	client->conn.w.pos = 0;

	if (strncmp(host, "unix:", 5) == 0) {
		type |= USOCK_UNIX;
		host += 5;
	}

	client->conn.sock.fd = usock(type, host, port);

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
	struct buffer_t *buf;

	request = json_create_object();
	json_add_string_to_object(request, "method", method);
	json_add_item_to_object(request, "params", params);
	json_add_number_to_object(request, "id", client->id);

	send_request(&client->conn, json_to_string(request));
	json_delete(request);

	// read
	conn = &client->conn;
	fd = conn->sock.fd;
	buf = &client->conn.r;

	for (;;) {
		if (buf->pos == (buf->size - 1)) {
			buf->size *= 2;
			new_buffer = realloc(buf->data, buf->size);
			if (new_buffer == NULL) {
				perror("Memory error");
				return -ENOMEM;
			}
			buf->data = new_buffer;
		}
		// can not fill the entire buffer, string must be NULL terminated
		max_read_size = buf->size - buf->pos - 1;
		if ((bytes_read = read(fd, buf->data + buf->pos,
						max_read_size)) == -1) {
			elog("read %d\n", strerror(errno));
			return -EIO;
		}
		if (!bytes_read) {
			// client closed the sending half of the connection
			if (client->conn.debug_level)
				dlog("Client closed connection.\n");
			return -EIO;
		}

		buf->pos += bytes_read;

		buf->data[buf->pos] = 0;
		if ((root = json_parse_stream(buf->data, &end_ptr)) != NULL) {
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
				buf->pos -= end_ptr - buf->data;
				memmove(buf->data, end_ptr, buf->pos);
				buf->data[buf->pos] = 0;

				*response = json_detach_item_from_object(root,
						"result");
				if (*response == NULL)
					goto out;

				json_delete(root);
				return 0;
			}
out:
			elog("INVALID JSON Received:\n---\n%s\n---\n",
			     buf->data);
			json_delete(root);
			return -EINVAL;
		} else if (end_ptr != (buf->data + buf->pos)) {
			// did we parse the all buffer? If so, just wait for more.
			// else there was an error before the buffer's end
			if (client->conn.debug_level) {
				elog("INVALID JSON Received:\n---\n%s\n---\n",
				     buf->data);
			}
			send_error(conn, JRPC_PARSE_ERROR,
					strdup("Parse error. Invalid JSON was "
						"received by the client."),
					NULL);
			return -EINVAL;
		}
	}
}
