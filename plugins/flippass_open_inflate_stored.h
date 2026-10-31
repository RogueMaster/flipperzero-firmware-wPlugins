#pragma once

/* Include after the private XML stage and CRC/output callbacks. */
#define FLIPPASS_OPEN_STORED_DEFLATE_CHUNK 128U

static bool fp_open_stored_read_exact(
    FlipPassOpenInflateReaderContext* reader,
    uint8_t* out,
    size_t data_size) {
    size_t offset = 0U;

    while(offset < data_size) {
        const size_t got = fp_open_payload_reader_read(out + offset, data_size - offset, reader);
        if(got == 0U) {
            reader->failed = true;
            reader->truncated = true;
            return false;
        }
        offset += got;
    }

    return true;
}

static bool fp_open_emit_stored_deflate(
    const FlipPassOpenInflateHostApiV1* host_api,
    const KDBXGzipMemberInfo* member_info,
    FlipPassOpenInflateXmlStageContext* stage,
    FuriString* error,
    bool* out_supported) {
    FlipPassOpenInflateReaderContext reader = {
        .host_api = host_api,
        .skip_bytes = member_info->body_offset,
        .remaining_bytes = member_info->compressed_size,
        .failed = false,
        .truncated = false,
    };
    uint8_t chunk[FLIPPASS_OPEN_STORED_DEFLATE_CHUNK];
    uint32_t crc32 = 0xFFFFFFFFU;
    size_t output_size = 0U;
    size_t consumed_size = 0U;
    bool final_block = false;
    bool failed = false;

    furi_assert(host_api);
    furi_assert(member_info);
    furi_assert(stage);
    furi_assert(out_supported);

    *out_supported = true;
    while(!final_block) {
        uint8_t header = 0U;
        uint8_t len_header[4];
        uint16_t len = 0U;
        uint16_t nlen = 0U;

        if(!fp_open_stored_read_exact(&reader, &header, 1U)) {
            break;
        }
        consumed_size++;
        final_block = (header & 0x01U) != 0U;
        if(((header >> 1U) & 0x03U) != 0U) {
            *out_supported = false;
            failed = true;
            goto cleanup;
        }
        /* Stored blocks discard the remaining bits up to the byte boundary. */
        *out_supported = true;

        if(!fp_open_stored_read_exact(&reader, len_header, sizeof(len_header))) {
            break;
        }
        consumed_size += sizeof(len_header);
        len = (uint16_t)len_header[0] | ((uint16_t)len_header[1] << 8U);
        nlen = (uint16_t)len_header[2] | ((uint16_t)len_header[3] << 8U);
        if((uint16_t)~len != nlen) {
            furi_string_set_str(error, "The stored GZip block length is invalid.");
            failed = true;
            goto cleanup;
        }
        if(output_size > member_info->expected_output_size ||
           len > (member_info->expected_output_size - output_size)) {
            furi_string_set_str(error, "The stored GZip payload exceeded the trailer size.");
            failed = true;
            goto cleanup;
        }

        while(len > 0U) {
            const size_t read_size = len > sizeof(chunk) ? sizeof(chunk) : (size_t)len;
            if(!fp_open_stored_read_exact(&reader, chunk, read_size)) {
                break;
            }
            consumed_size += read_size;
            crc32 = fp_open_crc32_update(crc32, chunk, read_size);
            if(!fp_open_stage_xml_output(chunk, read_size, stage)) {
                failed = true;
                goto cleanup;
            }
            output_size += read_size;
            len = (uint16_t)(len - read_size);
        }

        if(reader.failed) {
            break;
        }
    }

cleanup:
    memzero(chunk, sizeof(chunk));
    if(failed) return false;
#if FLIPPASS_ENABLE_LOGS || FLIPPASS_ENABLE_PROBE_LOGS
    if(host_api->log != NULL) {
        char log_line[192];
        snprintf(
            log_line,
            sizeof(log_line),
            "STREAM_GZIP_STORED_RESULT supported=%u failed=%u truncated=%u consumed=%lu out=%lu",
            *out_supported ? 1U : 0U,
            reader.failed ? 1U : 0U,
            reader.truncated ? 1U : 0U,
            (unsigned long)consumed_size,
            (unsigned long)output_size);
        host_api->log(host_api->context, log_line);
    }
#endif
    if(!*out_supported) {
        return false;
    }
    if(reader.failed || reader.truncated) {
        if(furi_string_empty(error)) {
            furi_string_set_str(error, "The stored GZip member is truncated.");
        }
        return false;
    }
    if(consumed_size != member_info->compressed_size ||
       output_size != member_info->expected_output_size) {
        furi_string_set_str(error, "The stored GZip payload did not match the trailer size.");
        return false;
    }
    crc32 = ~crc32;
    if(crc32 != member_info->expected_crc32) {
        furi_string_set_str(error, "The stored GZip CRC did not match the trailer.");
        return false;
    }

    return true;
}
