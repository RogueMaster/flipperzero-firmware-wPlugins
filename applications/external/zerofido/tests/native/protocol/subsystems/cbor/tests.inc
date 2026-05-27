static void test_cbor_text_read_rejects_declared_length_past_buffer(void) {
    static const uint8_t malformed[] = {0x7B, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    ZfCborCursor cursor;
    const uint8_t *text = NULL;
    size_t text_len = 0;

    zf_cbor_cursor_init(&cursor, malformed, sizeof(malformed));
    expect(!zf_cbor_read_text_ptr(&cursor, &text, &text_len),
           "text read should reject a declared length that exceeds the buffer");
}

static void test_cbor_skip_rejects_declared_length_past_buffer(void) {
    static const uint8_t malformed[] = {0x5A, 0xFF, 0xFF, 0xFF, 0xFF};
    ZfCborCursor cursor;

    zf_cbor_cursor_init(&cursor, malformed, sizeof(malformed));
    expect(!zf_cbor_skip(&cursor), "skip should reject a declared length that exceeds the buffer");
}

static void test_cbor_skip_accepts_single_precision_float(void) {
    static const uint8_t encoded[] = {0xFA, 0x3F, 0x80, 0x00, 0x00};
    ZfCborCursor cursor;

    zf_cbor_cursor_init(&cursor, encoded, sizeof(encoded));
    expect(zf_cbor_skip(&cursor), "skip should consume payload-carrying simple values");
    expect(cursor.ptr == cursor.end, "skip should consume the full float payload");
}

static void test_cbor_read_uint_rejects_noncanonical_encoding(void) {
    static const uint8_t malformed[] = {0x18, 0x17};
    ZfCborCursor cursor;
    uint64_t value = 0;

    zf_cbor_cursor_init(&cursor, malformed, sizeof(malformed));
    expect(!zf_cbor_read_uint(&cursor, &value),
           "uint read should reject non-canonical one-byte encodings");
}

static void test_cbor_read_text_rejects_noncanonical_length(void) {
    static const uint8_t malformed[] = {
        0x78, 0x17, 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
        'a',  'a',  'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a',
    };
    ZfCborCursor cursor;
    const uint8_t *text = NULL;
    size_t text_len = 0;

    zf_cbor_cursor_init(&cursor, malformed, sizeof(malformed));
    expect(!zf_cbor_read_text_ptr(&cursor, &text, &text_len),
           "text read should reject non-canonical length encodings");
}
