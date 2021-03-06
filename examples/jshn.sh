. ./jshn.sh

# generating json data
json_init
json_add_string "msg" "Hello, world!"
json_add_object "test"
json_add_int "testdata" "1"
json_close_object
MSG=`json_dump`
# MSG now contains: { "msg": "Hello, world!", "test": { "testdata": 1 } }


# parsing json data
json_load "$MSG"
json_select test
json_get_var var1 testdata
json_select ..
json_get_var var2 msg
echo "msg: $var2 - testdata: $var1"
