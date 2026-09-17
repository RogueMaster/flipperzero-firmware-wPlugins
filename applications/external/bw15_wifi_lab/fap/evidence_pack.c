#include "evidence_pack.h"

static void discard_if_possible(G5EvidenceWriter* writer) {
    if(writer && writer->discard) writer->discard(writer->context);
}

G5EvidenceResult g5_evidence_commit_buffer(
    G5EvidenceWriter* writer,
    const uint8_t* data,
    size_t length) {
    if(!writer || !data || length == 0 || !writer->write || !writer->sync || !writer->close ||
       !writer->commit || !writer->discard)
        return G5EvidenceInvalidArgument;

    size_t offset = 0;
    while(offset < length) {
        const size_t written = writer->write(writer->context, data + offset, length - offset);
        if(written == 0 || written > length - offset) {
            writer->close(writer->context);
            discard_if_possible(writer);
            return G5EvidenceWriteFailed;
        }
        offset += written;
    }

    if(!writer->sync(writer->context)) {
        writer->close(writer->context);
        discard_if_possible(writer);
        return G5EvidenceSyncFailed;
    }
    if(!writer->close(writer->context)) {
        discard_if_possible(writer);
        return G5EvidenceCloseFailed;
    }
    if(!writer->commit(writer->context)) {
        discard_if_possible(writer);
        return G5EvidenceCommitFailed;
    }
    return G5EvidenceOk;
}

bool g5_evidence_json_escape(
    const char* input,
    char* output,
    size_t output_capacity,
    size_t* output_length) {
    static const char hex[] = "0123456789ABCDEF";
    if(output_length) *output_length = 0;
    if(!input || !output || output_capacity == 0 || !output_length) return false;

    size_t used = 0;
    for(const unsigned char* p = (const unsigned char*)input; *p; ++p) {
        char encoded[6];
        size_t encoded_length = 1;
        encoded[0] = (char)*p;
        if(*p == '"' || *p == '\\') {
            encoded[0] = '\\';
            encoded[1] = (char)*p;
            encoded_length = 2;
        } else if(*p == '\b' || *p == '\f' || *p == '\n' || *p == '\r' || *p == '\t') {
            encoded[0] = '\\';
            encoded[1] = (*p == '\b') ? 'b' : (*p == '\f') ? 'f' : (*p == '\n') ? 'n' :
                         (*p == '\r')   ? 'r' :
                                         't';
            encoded_length = 2;
        } else if(*p < 0x20) {
            encoded[0] = '\\';
            encoded[1] = 'u';
            encoded[2] = '0';
            encoded[3] = '0';
            encoded[4] = hex[*p >> 4];
            encoded[5] = hex[*p & 0x0F];
            encoded_length = 6;
        }
        if(encoded_length > output_capacity - used - 1) return false;
        for(size_t i = 0; i < encoded_length; ++i) output[used++] = encoded[i];
    }
    output[used] = '\0';
    *output_length = used;
    return true;
}

const char* g5_evidence_result_name(G5EvidenceResult result) {
    switch(result) {
    case G5EvidenceOk: return "ok";
    case G5EvidenceInvalidArgument: return "invalid_argument";
    case G5EvidenceWriteFailed: return "write_failed";
    case G5EvidenceSyncFailed: return "sync_failed";
    case G5EvidenceCloseFailed: return "close_failed";
    case G5EvidenceCommitFailed: return "commit_failed";
    }
    return "unknown";
}

