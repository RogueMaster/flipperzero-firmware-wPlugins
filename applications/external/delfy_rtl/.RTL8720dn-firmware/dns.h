#ifndef DELFTY_DNS_H
#define DELFTY_DNS_H

#define DNS_HEADER_SIZE        12
#define DNS_SERVER_PORT        53

#include <Arduino.h>

//DNS

#include <lwip/udp.h>
#define DNS_QR_QUERY           0
#define DNS_QR_RESPONSE        1
#define DNS_OPCODE_QUERY       0
#define DNS_DEFAULT_TTL        60  // Default Time To Live : time interval in seconds that the resource record should be cached before being discarded
#define DNS_HEADER_SIZE        12
#define DNS_OFFSET_DOMAIN_NAME DNS_HEADER_SIZE  // Offset in bytes to reach the domain name labels in the DNS message
#define DNS_DEFAULT_PORT       53

enum class DNSReplyCode : uint16_t {
  NoError = 0,
  FormError = 1,
  ServerFailure = 2,
  NonExistentDomain = 3,
  NotImplemented = 4,
  Refused = 5,
  YXDomain = 6,
  YXRRSet = 7,
  NXRRSet = 8
};

enum DNSType {
  DNS_TYPE_A = 1,      // Host Address
  DNS_TYPE_AAAA = 28,  // IPv6 Address
  DNS_TYPE_SOA = 6,    // Start Of a zone of Authority
  DNS_TYPE_PTR = 12,   // Domain name PoinTeR
  DNS_TYPE_DNAME = 39  // Delegation Name
};
  
enum DNSClass {
  DNS_CLASS_IN = 1,  // INternet
  DNS_CLASS_CH = 3   // CHaos
};
  
enum DNSRDLength {
  DNS_RDLENGTH_IPV4 = 4  // 4 bytes for an IPv4 address
};


struct dns_hdr {
  PACK_STRUCT_FIELD(u16_t id);
  PACK_STRUCT_FIELD(u8_t flags1);
  PACK_STRUCT_FIELD(u8_t flags2);
  PACK_STRUCT_FIELD(u16_t numquestions);
  PACK_STRUCT_FIELD(u16_t numanswers);
  PACK_STRUCT_FIELD(u16_t numauthrr);
  PACK_STRUCT_FIELD(u16_t numextrarr);
} PACK_STRUCT_STRUCT;

struct DNSHeader {
  uint16_t ID;  // identification number
  union {
    struct {
      uint16_t RD     : 1;  // recursion desired
      uint16_t TC     : 1;  // truncated message
      uint16_t AA     : 1;  // authoritative answer
      uint16_t OPCode : 4;  // message_type
      uint16_t QR     : 1;  // query/response flag
      uint16_t RCode  : 4;  // response code
      uint16_t Z      : 3;  // its z! reserved
      uint16_t RA     : 1;  // recursion available
    };
    uint16_t Flags;
  };
  uint16_t QDCount;  // number of question entries
  uint16_t ANCount;  // number of ANswer entries
  uint16_t NSCount;  // number of authority entries
  uint16_t ARCount;  // number of Additional Resource entries
};

struct DNSQuestion {
  const uint8_t *QName;
  uint16_t QNameLength;
  uint16_t QType;
  uint16_t QClass;
};

bool requestIncludesOnlyOneQuestion(DNSHeader &dnsHeader);
void replyWithIP(DNSHeader &dnsHeader, DNSQuestion &dnsQuestion);


//static void dnss_receive_udp_packet_handler(void *arg,struct udp_pcb *udp_pcb,struct pbuf *udp_packet_buffer,struct ip_addr *sender_addr,uint16_t sender_port) ;
void start_DNS_Server();
void unbind_dns();
void unbind_all_udp();
#endif