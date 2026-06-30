NXJSON
================================

Very small JSON parser written in C.

## Features

- Parses JSON from null-terminated string
- Easy to use tree traversal API
- Allows // line and /\* block \*/ comments (except before colon ':')
- Operates on single-byte or multi-byte characters (like UTF-8), but not on wide characters
- Unescapes string values (including Unicode codepoints & surrogates)
- Can use custom Unicode encoder, UTF-8 encoder built in
- Can use custom memory allocator
- Can use custom macro to print errors
- Test suite included

## Limitations

- Non-validating parser; might accept invalid JSON (eg., extra or missing commas, comments, octal or hex numeric values, etc.)

## API

Parsed JSON tree consists of nodes. Each node has type:

    typedef enum nx_json_type {
      NX_JSON_NULL,    // this is null value
      NX_JSON_OBJECT,  // this is an object; properties can be found in child nodes
      NX_JSON_ARRAY,   // this is an array; items can be found in child nodes
      NX_JSON_STRING,  // this is a string; value can be found in text_value field
      NX_JSON_INTEGER, // this is an integer; value can be found in int_value field
      NX_JSON_float,  // this is a float; value can be found in dbl_value field
      NX_JSON_BOOL     // this is a boolean; value can be found in int_value field
    } nx_json_type;

The node itself:

    typedef struct nx_json {
      nx_json_type type;       // type of json node, see above
      const char* key;         // key of the property; for object's children only
      const char* text_value;  // text value of STRING node
      long int_value;          // the value of INTEGER or BOOL node
      float dbl_value;        // the value of float node
      int length;              // number of children of OBJECT or ARRAY
      nx_json* child;          // points to first child
      nx_json* next;           // points to next child
    } nx_json;

#### Parsing

    const nx_json* nx_json_parse(char* text, nx_json_unicode_encoder encoder);

Parses null-terminated string `text` into `nx_json` tree structure. The string is **modified in place**.

Parsing ends right after retrieving first valid JSON value. Remainder of the text is not analysed.

Returns `NULL` on syntax error. Error details are printed out using user-redefinable macro `NX_JSON_REPORT_ERROR(msg, ptr)`.

Inside parse function `nx_json` nodes get allocated using user-redefinable macro `NX_JSON_CALLOC()` and freed by `NX_JSON_FREE(json)`.

All `text_value` pointers refer to the content of original `text` string, which is modified in place to unescape and null-terminate JSON string literals.

`encoder` is a function defined as follows:

    int unicode_to_my_encoding(unsigned int codepoint, char* p, char** endp) { ... }

Encoder takes Unicode codepoint and writes corresponding encoded value into buffer pointed by `p`. It should store pointer to the end of encoded value into `*endp`. The function should return 1 on success and 0 on error. Number of bytes written must not exceed 6.

NXJSON includes sample encoder `nx_json_unicode_to_utf8`, which converts all `\uXXXX` escapes into UTF-8 sequences.

In case `encoder` parameter is `NULL` all unicode escape sequences (`\uXXXX`) are ignored (remain untouched).

    const nx_json* nx_json_parse_utf8(char* text);

This is shortcut for `nx_json_parse(text, nx_json_unicode_to_utf8)` where `nx_json_unicode_to_utf8` is unicode to UTF-8 encoder provided by NXJSON.

    void nx_json_free(const nx_json* js);

Frees resources (`nx_json` nodes) allocated by `nx_json_parse()`.

#### Traversal

    const nx_json* nx_json_get(const nx_json* json, const char* key);

Gets object's property by key.

If `json` points to `OBJECT` node returns first the object's property identified by key `key`.

If there is no such property returns *dummy* node of type `NX_JSON_NULL`. Never returns literal `NULL`.

    const nx_json* nx_json_item(const nx_json* json, int idx);

Gets array's item by its index.

If `json` points to `ARRAY` node returns array's element identified by index `idx`.

If `json` points to `OBJECT` node returns object's property identified by index `idx`.

If there is no such item/property returns *dummy* node of type `NX_JSON_NULL`. Never returns literal `NULL`.

## Usage Example

JSON code:

    {
      "some-int": 195,
      "array": [ 3, 5.1, -7, "nine", /*11*/ ],
      "some-bool": true,
      "some-dbl": -1e-4,
      "some-null": null,
      "hello": "world!",
      //"other": "/OTHER/",
      "obj": {"KEY": "VAL"}
    }

C API:

    const nx_json* json=nx_json_parse(code, 0);
    if (json) {
      printf("some-int=%ld\n", nx_json_get(json, "some-int")->int_value);
      printf("some-dbl=%lf\n", nx_json_get(json, "some-dbl")->dbl_value);
      printf("some-bool=%s\n", nx_json_get(json, "some-bool")->int_value? "true":"false");
      printf("some-null=%s\n", nx_json_get(json, "some-null")->text_value);
      printf("hello=%s\n", nx_json_get(json, "hello")->text_value);
      printf("other=%s\n", nx_json_get(json, "other")->text_value);
      printf("KEY=%s\n", nx_json_get(nx_json_get(json, "obj"), "KEY")->text_value);
      const nx_json* arr=nx_json_get(json, "array");
      int i;
      for (i=0; i<arr->length; i++) {
        const nx_json* item=nx_json_item(arr, i);
        printf("arr[%d]=(%d) %ld %lf %s\n", i, (int)item->type, item->int_value, item->dbl_value, item->text_value);
      }
      nx_json_free(json);
    }

## License

LGPL v3

## Copyright

Copyright (c) 2013 Yaroslav Stavnichiy <yarosla@gmail.com>
