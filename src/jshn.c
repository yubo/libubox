/*
 * Copyright (C) 2011-2013 Felix Fietkau <nbd@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Copyright 2016 yubo. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * Yu Bo <yubo@xiaomi.com>
 */

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <getopt.h>

#include "list.h"
#include "json.h"
#include "blob.h"
#include "blobmsg_json.h"

#define MAX_VARLEN	256

static struct blob_buf b = { 0 };

static const char *var_prefix = "";
static int var_prefix_len = 0;

static int add_json_element(const char *key, struct json *obj);

static int add_json_object(struct json *obj)
{
	int ret = 0;

	json_object_object_foreach(obj, key, val) {
		ret = add_json_element(key, val);
		if (ret)
			break;
	}
	return ret;
}

static int add_json_array(struct json *a)
{
	struct json *val;
	char seq[12];
	int i, ret;

	for (i = 0, val = a->child; val; val = val->next, i++){
		sprintf(seq, "%d", i);
		ret = add_json_element(seq, val);
		if (ret)
			return ret;
	}
	return 0;
}

static void add_json_string(const char *str)
{
	char *ptr = (char *) str;
	int len;
	char *c;

	while ((c = strchr(ptr, '\'')) != NULL) {
		len = c - ptr;
		if (len > 0)
			fwrite(ptr, len, 1, stdout);
		ptr = c + 1;
		c = "'\\''";
		fwrite(c, strlen(c), 1, stdout);
	}
	len = strlen(ptr);
	if (len > 0)
		fwrite(ptr, len, 1, stdout);
}

static void write_key_string(const char *key)
{
	while (*key) {
		putc(isalnum(*key) ? *key : '_', stdout);
		key++;
	}
}

static int add_json_element(const char *key, struct json *obj)
{
	char *type;

	if (!obj)
		return -1;

	switch (json_object_get_type(obj)) {
	case JSON_T_OBJECT:
		type = "object";
		break;
	case JSON_T_ARRAY:
		type = "array";
		break;
	case JSON_T_STRING:
		type = "string";
		break;
	case JSON_T_TRUE:
	case JSON_T_FALSE:
		type = "boolean";
		break;
	case JSON_T_NUMBER:
		if (json_type_is_double(obj))
			type = "double";
		else
			type = "int";
		break;
	default:
		return -1;
	}

	fprintf(stdout, "json_add_%s '", type);
	write_key_string(key);

	switch (json_object_get_type(obj)) {
	case JSON_T_OBJECT:
		fprintf(stdout, "';\n");
		add_json_object(obj);
		fprintf(stdout, "json_close_object;\n");
		break;
	case JSON_T_ARRAY:
		fprintf(stdout, "';\n");
		add_json_array(obj);
		fprintf(stdout, "json_close_array;\n");
		break;
	case JSON_T_STRING:
		fprintf(stdout, "' '");
		add_json_string(obj->valuestring);
		fprintf(stdout, "';\n");
		break;
	case JSON_T_TRUE:
		fprintf(stdout, "' 1;\n");
		break;
	case JSON_T_FALSE:
		fprintf(stdout, "' 0;\n");
		break;
	case JSON_T_NUMBER:
		if (json_type_is_double(obj))
			fprintf(stdout, "' %lf;\n", obj->valuedouble);
		else
			fprintf(stdout, "' ""%" PRId64 " ;\n", obj->valueint);
		break;
	default:
		return -1;
	}

	return 0;
}

static int jshn_parse(const char *str)
{
	struct json *obj;

	obj = json_parse(str);
	if (!obj || obj->type != JSON_T_OBJECT) {
		fprintf(stderr, "Failed to parse message data\n");
		return 1;
	}
	fprintf(stdout, "json_init;\n");
	add_json_object(obj);
	fflush(stdout);

	return 0;
}

static char *get_keys(const char *prefix)
{
	char *keys;

	keys = alloca(var_prefix_len + strlen(prefix) + sizeof("K_") + 1);
	sprintf(keys, "%sK_%s", var_prefix, prefix);
	return getenv(keys);
}

static void get_var(const char *prefix, const char **name, char **var, char **type)
{
	char *tmpname, *varname;

	tmpname = alloca(var_prefix_len + strlen(prefix) + 1 + strlen(*name) + 1 + sizeof("T_"));

	sprintf(tmpname, "%s%s_%s", var_prefix, prefix, *name);
	*var = getenv(tmpname);

	sprintf(tmpname, "%sT_%s_%s", var_prefix, prefix, *name);
	*type = getenv(tmpname);

	sprintf(tmpname, "%sN_%s_%s", var_prefix, prefix, *name);
	varname = getenv(tmpname);
	if (varname)
		*name = varname;
}

static struct json *jshn_add_objects(struct json *obj, const char *prefix, bool array);

static void jshn_add_object_var(struct json *obj, bool array, const char *prefix, const char *name)
{
	struct json *new;
	char *var, *type;

	get_var(prefix, &name, &var, &type);
	if (!var || !type)
		return;

	if (!strcmp(type, "array")) {
		new = json_object_new_array();
		jshn_add_objects(new, var, true);
	} else if (!strcmp(type, "object")) {
		new = json_object_new_object();
		jshn_add_objects(new, var, false);
	} else if (!strcmp(type, "string")) {
		new = json_object_new_string(var);
	} else if (!strcmp(type, "int")) {
		new = json_object_new_int(atoi(var));
	} else if (!strcmp(type, "double")) {
		new = json_object_new_double(strtod(var, NULL));
	} else if (!strcmp(type, "boolean")) {
		if (!!atoi(var))
			new = json_create_true();
		else
			new = json_create_false();
	} else {
		return;
	}

	if (array)
		json_object_array_add(obj, new);
	else
		json_object_object_add(obj, name, new);
}

static struct json *jshn_add_objects(struct json *obj, const char *prefix, bool array)
{
	char *keys, *key, *brk;

	keys = get_keys(prefix);
	if (!keys || !obj)
		goto out;

	for (key = strtok_r(keys, " ", &brk); key;
	     key = strtok_r(NULL, " ", &brk)) {
		jshn_add_object_var(obj, array, prefix, key);
	}

out:
	return obj;
}

static int jshn_format(bool no_newline, bool indent)
{
	struct json *obj;
	const char *output;
	char *blobmsg_output = NULL;
	int ret = -1;

	if (!(obj = json_object_new_object()))
		return -1;

	jshn_add_objects(obj, "J_V", false);
	if (!(output = json_to_string(obj)))
		goto out;

	if (indent) {
		blob_buf_init(&b, 0);
		if (!blobmsg_add_json_from_string(&b, output))
			goto out;
		if (!(blobmsg_output = blobmsg_format_json_indent(b.head, 1, 0)))
			goto out;
		output = blobmsg_output;
	}
	fprintf(stdout, "%s%s", output, no_newline ? "" : "\n");
	free(blobmsg_output);
	ret = 0;

out:
	json_object_put(obj);
	return ret;
}

static int usage(const char *progname)
{
	fprintf(stderr, "Usage: %s [-n] [-i] -r <message>|-w\n", progname);
	return 2;
}

int main(int argc, char **argv)
{
	bool no_newline = false;
	bool indent = false;
	int ch;

	while ((ch = getopt(argc, argv, "p:nir:w")) != -1) {
		switch(ch) {
		case 'p':
			var_prefix = optarg;
			var_prefix_len = strlen(var_prefix);
			break;
		case 'r':
			return jshn_parse(optarg);
		case 'w':
			return jshn_format(no_newline, indent);
		case 'n':
			no_newline = true;
			break;
		case 'i':
			indent = true;
			break;
		default:
			return usage(argv[0]);
		}
	}
	return usage(argv[0]);
}
