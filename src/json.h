/*
 * Copyright 2016 yubo. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */ 
#ifndef __JSON_H__
#define __JSON_H__

/* json Types: */
#define JSON_T_FALSE 0
#define JSON_T_TRUE 1
#define JSON_T_NULL 2
#define JSON_T_NUMBER 3
#define JSON_T_STRING 4
#define JSON_T_ARRAY 5
#define JSON_T_OBJECT 6


#define JSON_T_IS_REFERENCE 256


/* The json structure: */
struct json {
	struct json *next, *prev;	/* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
	struct json *child;	/* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */

	int type;		/* The type of the item, as above. */

	char *valuestring;	/* The item's string, if type==json_String */
	int valueint;		/* The item's number, if type==json_Number */
	double valuedouble;	/* The item's number, if type==json_Number */

	char *string;		/* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
	char *print_out;
	int print_fmt;
};

struct json_hooks {
	void *(*malloc_fn) (size_t sz);
	void (*free_fn) (void *ptr);
};

/* Supply malloc, realloc and free functions to json */
extern void json_init_hooks(struct json_hooks *hooks);

/* Supply a block of JSON, and this returns a json object you can interrogate.
 * Call json_Delete when finished. */
extern struct json *json_parse(const char *value);

/* Supply a block of JSON, and this returns a json object you can interrogate.
 * Call json_Delete when finished.
 * end_ptr will point to 1 past the end of the JSON object */
extern struct json *json_parse_stream(const char *value, char **end_ptr);

/* Render a json entity to text for transfer/storage. will Free the char* when delete(item). */
extern char *json_to_string(struct json *item);

/* Render a json entity to text for transfer/storage without any formatting.
 * will Free the char* when delete(item). */
extern char *json_to_string_unformatted(struct json *item);

/* Delete a json entity and all subentities. */
extern void json_delete(struct json *c);

/* Returns the number of items in an array (or object). */
extern int json_get_array_size(struct json *array);

/* Retrieve item number "item" from array "array". Returns NULL if unsuccessful. */
extern struct json *json_get_array_item(struct json *array, int item);

/* Get item "string" from object. Case insensitive. */
extern struct json *json_get_object_item(struct json *object, const char *string);

/* These calls create a json item of the appropriate type. */
extern struct json *json_create_null(void);
extern struct json *json_create_true(void);
extern struct json *json_create_false(void);
extern struct json *json_create_bool(int b);
extern struct json *json_create_number(double num);
extern struct json *json_create_string(const char *string);
extern struct json *json_create_array(void);
extern struct json *json_create_object(void);

/* These utilities create an Array of count items. */
extern struct json *json_create_int_array(int *numbers, int count);
extern struct json *json_create_float_array(float *numbers, int count);
extern struct json *json_create_double_array(double *numbers, int count);
extern struct json *json_create_string_array(const char **strings, int count);

/* Append item to the specified array/object. */
extern void json_add_item_to_array(struct json *array, struct json *item);
extern void json_add_item_to_object(struct json *object,
				     const char *string, struct json *item);
/*
 * Append reference to item to the specified array/object. 
 * Use this when you want to add an existing json to a new json,
 * but don't want to corrupt your existing json.
 */
extern void json_add_item_reference_to_array(struct json *array,
					      struct json *item);
extern void json_add_item_reference_to_object(struct json *object,
					       const char *string, struct json *item);

/* Remove/Detatch items from Arrays/Objects. */
extern struct json *json_detach_item_from_array(struct json *array, int which);
extern void json_delete_item_from_array(struct json *array, int which);
extern struct json *json_detach_item_from_object(struct json *object,
					    const char *string);
extern void json_delete_item_from_object(struct json *object,
					  const char *string);

/* Update array items. */
extern void json_replace_item_in_array(struct json *array, int which,
				       struct json *newitem);
extern void json_replace_item_in_object(struct json *object,
					const char *string,
					struct json *newitem);

void *(*json_malloc) (size_t sz);
void (*json_free) (void *ptr);

#define json_add_null_to_object(object,name)     json_add_item_to_object(object, name, json_create_null())
#define json_add_true_to_object(object,name)     json_add_item_to_object(object, name, json_create_true())
#define json_add_false_to_object(object,name)    json_add_item_to_object(object, name, json_create_false())
#define json_add_number_to_object(object,name,n) json_add_item_to_object(object, name, json_create_number(n))
#define json_add_string_to_object(object,name,s) json_add_item_to_object(object, name, json_create_string(s))

/* compatible json-c */
#define json_object_object_foreach(obj,key,val) \
	char *key = NULL;                       \
	struct json *val;                       \
	for(val = obj->child,                   \
		key = val ? val->string : NULL; \
		val ; val = val->next,          \
		key = val ? val->string : NULL)

#define json_object_get_type(o) (o)->type

#define json_type_null JSON_T_NULL
#define json_type_double JSON_T_NUMBER
#define json_type_int JSON_T_NUMBER
#define json_type_object JSON_T_OBJECT
#define json_type_array JSON_T_ARRAY
#define json_type_string JSON_T_STRING

#define array_list_length(a) json_get_array_size(a)
#define json_object_new_object json_create_object
#define json_object_new_array json_create_array
#define json_object_new_string json_create_string
#define json_object_new_int json_create_number
#define json_object_new_double json_create_number
#define json_object_array_add  json_add_item_to_array
#define json_object_object_add  json_add_item_to_object
#define json_object_object_add  json_add_item_to_object
#define json_object_to_json_string json_to_string

/* warning! need references count */
#define json_object_put(obj) json_delete(obj)

/* from json-c/json.h */
#define JSON_FILE_BUF_SIZE 4096
extern struct json* json_object_from_file(const char *filename);
extern struct json* json_object_from_fd(int fd);
extern int json_object_to_file(const char *filename, struct json *obj);
extern int json_object_to_file_ext(const char *filename, struct json *obj, int flags);
extern int json_parse_int64(const char *buf, int64_t *retval);
extern int json_parse_double(const char *buf, double *retval);
extern const char *json_type_to_name(int type);

extern int json_type_is_double(struct json *item);

#endif
