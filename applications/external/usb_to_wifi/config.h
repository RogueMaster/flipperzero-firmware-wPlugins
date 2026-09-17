#pragma once

/* Wire-critical limits shared conceptually with macOS BridgeConfiguration. */
#define FIB_PROTOCOL_MAJOR             1U
#define FIB_PROTOCOL_MINOR             0U
#define FIB_MAX_FRAME_PAYLOAD          512U
#define FIB_MIN_NEGOTIATED_PAYLOAD     28U
#define FIB_RESPONSE_CHUNK_SIZE        192U
#define FIB_MAX_URL_LENGTH             384U
#define FIB_MAX_REQUEST_HEADERS        8U
#define FIB_MAX_HEADER_NAME_LENGTH     64U
#define FIB_MAX_HEADER_VALUE_LENGTH    256U
#define FIB_MAX_REQUEST_HEADER_BYTES   1024U
#define FIB_MAX_REQUEST_BODY_SIZE      4096U
#define FIB_MAX_RESPONSE_SIZE          (4U * 1024U * 1024U)
#define FIB_RESPONSE_PREVIEW_SIZE      1536U
#define FIB_MAX_REDIRECTS              3U
#define FIB_DEFAULT_REQUEST_TIMEOUT_MS 25000U
#define FIB_MAX_REQUEST_TIMEOUT_MS     30000U
#define FIB_FRAME_ASSEMBLY_TIMEOUT_MS  5000U
#define FIB_HANDSHAKE_TIMEOUT_MS       5000U
#define FIB_PONG_TIMEOUT_MS            2000U
#define FIB_IDLE_TIMEOUT_MS            30000U
#define FIB_MAX_ERROR_DETAIL           128U

#define FIB_APP_VERSION            "0.4.0"
#define FIB_CDC_INTERFACE          1U
#define FIB_CDC_RX_STREAM_SIZE     4096U
#define FIB_TRANSPORT_WORKER_STACK 1536U
