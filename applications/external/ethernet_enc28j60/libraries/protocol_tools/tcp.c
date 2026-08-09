#include "app_user.h"
#include "tcp.h"
#include "ipv4.h"
#include "ethernet_protocol.h"

// New function to create a complete TCP packet
bool create_tcp_header(
    tcp_header_t* tcp_header,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number,
    uint16_t flags,
    uint16_t window_size,
    uint16_t urgent_pointer) {
    if(tcp_header == NULL) return false;

    // Set source and destination ports
    uint_to_bytes(&source_port, tcp_header->source_port, sizeof(uint16_t));
    uint_to_bytes(&dest_port, tcp_header->dest_port, sizeof(uint16_t));

    // Set sequence and acknowledgment numbers
    uint_to_bytes(&sequence, tcp_header->sequence, sizeof(uint32_t));
    uint_to_bytes(&ack_number, tcp_header->ack_number, sizeof(uint32_t));

    uint16_t offset_and_flags = ((5 << 12) | flags); // 5 = header sin opciones

    uint_to_bytes(&offset_and_flags, tcp_header->data_offset_flags, sizeof(uint16_t));

    // Set window size
    uint_to_bytes(&window_size, tcp_header->window_size, sizeof(uint16_t));

    // Checksum will be calculated later
    tcp_header->checksum[0] = 0;
    tcp_header->checksum[1] = 0;

    // Set urgent pointer
    uint_to_bytes(&urgent_pointer, tcp_header->urgent_pointer, sizeof(uint16_t));

    return true;
}

void calculate_checksum_tcp(
    uint16_t options_size,
    pseudo_header_ip_t* pseudo_header,
    tcp_header_t* tcp_header) {
    uint16_t tcp_header_len = options_size + TCP_HEADER_LEN;

    uint8_t* buffer_checksum = calloc(1, IP_PSEUDO_HEADER_LEN + tcp_header_len);

    uint_to_bytes(&tcp_header_len, pseudo_header->tcp_lenght, sizeof(uint16_t));

    memcpy(buffer_checksum, pseudo_header, IP_PSEUDO_HEADER_LEN);
    memcpy(buffer_checksum + IP_PSEUDO_HEADER_LEN, tcp_header, tcp_header_len);

    uint16_t checksum = calculate_checksum_ipv4(
        (uint16_t*)buffer_checksum, (IP_PSEUDO_HEADER_LEN + tcp_header_len) / 2);

    uint_to_bytes(&checksum, tcp_header->checksum, sizeof(uint16_t));

    free(buffer_checksum);
}

bool set_tcp_header_syn(
    uint8_t* buffer,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number,
    uint16_t window_size,
    uint16_t urgent_pointer,
    uint16_t* len) {
    if(buffer == NULL || source_ip == NULL || target_ip == NULL) return false;

    pseudo_header_ip_t pseudo_header;
    memset(&pseudo_header, 0, sizeof(pseudo_header_ip_t));
    memcpy(pseudo_header.source_ip, source_ip, 4);
    memcpy(pseudo_header.target_ip, target_ip, 4);
    pseudo_header.protocol = 0x06;

    tcp_header_t tcp_header;
    memset(&tcp_header, 0, sizeof(tcp_header_t));

    create_tcp_header(
        &tcp_header,
        source_port,
        dest_port,
        sequence,
        ack_number,
        TCP_SYN,
        window_size,
        urgent_pointer);

    uint16_t idx = 0;

    // MSS 1460
    tcp_header.options[idx++] = 0x02;
    tcp_header.options[idx++] = 0x04;
    tcp_header.options[idx++] = 0x05;
    tcp_header.options[idx++] = 0xB4;

    // NOP, NOP
    tcp_header.options[idx++] = 0x01;
    tcp_header.options[idx++] = 0x01;

    // SACK permitted
    tcp_header.options[idx++] = 0x04;
    tcp_header.options[idx++] = 0x02;

    // NOP, NOP
    tcp_header.options[idx++] = 0x01;
    tcp_header.options[idx++] = 0x01;

    // Timestamp
    tcp_header.options[idx++] = 0x08;
    tcp_header.options[idx++] = 0x0A;

    // TSval dinámico
    uint32_t ts = furi_get_tick();
    uint_to_bytes(&ts, &tcp_header.options[idx], 4);
    idx += 4;

    // TSecr = 0
    uint32_t tsecr = 0;
    uint_to_bytes(&tsecr, &tcp_header.options[idx], 4);
    idx += 4;

    // Padding a múltiplo de 4
    while(idx % 4 != 0) {
        tcp_header.options[idx++] = TCP_NOP;
    }

    uint16_t options_size = idx;

    uint8_t header_words = (TCP_HEADER_LEN + options_size) / 4;
    uint16_t offset_and_flags;
    bytes_to_uint(&offset_and_flags, tcp_header.data_offset_flags, sizeof(uint16_t));

    offset_and_flags &= 0x0FFF; // limpiar solo data offset
    offset_and_flags |= (header_words << 12);

    uint_to_bytes(&offset_and_flags, tcp_header.data_offset_flags, sizeof(uint16_t));

    calculate_checksum_tcp(options_size, &pseudo_header, &tcp_header);

    memcpy(buffer, &tcp_header, TCP_HEADER_LEN + options_size);

    *len = TCP_HEADER_LEN + options_size;

    return true;
}

bool set_tcp_header_fin(
    uint8_t* buffer,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number,
    uint16_t window_size,
    uint16_t urgent_pointer,
    uint16_t* len) {
    if(buffer == NULL) return false;

    pseudo_header_ip_t* pseudo_header = calloc(1, sizeof(pseudo_header_ip_t));
    memcpy(pseudo_header->source_ip, source_ip, 4);
    memcpy(pseudo_header->target_ip, target_ip, 4);
    pseudo_header->protocol = 0x06;

    tcp_header_t* tcp_header = calloc(1, sizeof(tcp_header_t));

    if(!create_tcp_header(
           tcp_header,
           source_port,
           dest_port,
           sequence,
           ack_number,
           TCP_FIN,
           window_size,
           urgent_pointer))
        return false;

    calculate_checksum_tcp(0, pseudo_header, tcp_header);

    memcpy(buffer, tcp_header, TCP_HEADER_LEN);

    free(tcp_header);

    *len = TCP_HEADER_LEN;

    return true;
}

bool set_tcp_header_ack(
    uint8_t* buffer,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number,
    uint16_t window_size,
    uint16_t urgent_pointer,
    uint16_t* len) {
    if(buffer == NULL) return false;

    pseudo_header_ip_t* pseudo_header = calloc(1, sizeof(pseudo_header_ip_t));
    memcpy(pseudo_header->source_ip, source_ip, 4);
    memcpy(pseudo_header->target_ip, target_ip, 4);
    pseudo_header->protocol = 0x06;

    tcp_header_t* tcp_header = calloc(1, sizeof(tcp_header_t));

    if(!create_tcp_header(
           tcp_header,
           source_port,
           dest_port,
           sequence,
           ack_number,
           TCP_ACK,
           window_size,
           urgent_pointer))
        return false;

    calculate_checksum_tcp(0, pseudo_header, tcp_header);

    memcpy(buffer, tcp_header, TCP_HEADER_LEN);

    free(tcp_header);

    *len = TCP_HEADER_LEN;

    return true;
}

bool set_tcp_header_tseq(
    uint8_t* buffer,
    uint8_t* source_ip,
    uint8_t* target_ip,
    uint16_t source_port,
    uint16_t dest_port,
    uint32_t sequence,
    uint32_t ack_number,
    uint16_t window_size,
    uint16_t urgent_pointer,
    uint16_t* options_size,
    uint8_t* options_vector,
    uint16_t* len) {
    pseudo_header_ip_t pseudo_header;
    memcpy(pseudo_header.source_ip, source_ip, 4);
    memcpy(pseudo_header.target_ip, target_ip, 4);
    pseudo_header.protocol = 0x06;

    tcp_header_t tcp_header;
    memset(&tcp_header, 0, sizeof(tcp_header_t));

    create_tcp_header(
        &tcp_header,
        source_port,
        dest_port,
        sequence,
        ack_number,
        TCP_SYN,
        window_size,
        urgent_pointer);

    memcpy(tcp_header.options, options_vector, *options_size);

    tcp_header.data_offset_flags[0] =
        ((tcp_header.data_offset_flags[0] >> 4) + ((*options_size) / 4)) << 4;

    calculate_checksum_tcp(*options_size, &pseudo_header, &tcp_header);

    memcpy(buffer, &tcp_header, TCP_HEADER_LEN + *options_size);

    *len = TCP_HEADER_LEN + *options_size;

    return true;
}

tcp_header_t tcp_get_header(uint8_t* buffer) {
    tcp_header_t header = {0};

    memcpy(&header, buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN, sizeof(tcp_header_t) - 50);

    uint8_t data_offset = (header.data_offset_flags[0] >> 4) * 4;

    memcpy(&header, buffer + ETHERNET_HEADER_LEN + IP_HEADER_LEN, data_offset);

    return header;
}

bool is_tcp(uint8_t* buffer) {
    if(buffer == NULL) return false;

    // Check if it's an IPv4 packet first
    if(!is_ipv4(buffer)) return false;

    // Get IP header and check protocol field
    ipv4_header_t ip_header = ipv4_get_header(buffer);
    return ip_header.protocol == 6;
}
