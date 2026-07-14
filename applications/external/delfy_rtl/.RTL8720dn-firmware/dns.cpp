#include "dns.h"
#define DNS_QR_RESPONSE        1

uint8_t dhcpServer[4]={192,168,1,1};

bool requestIncludesOnlyOneQuestion(DNSHeader &dnsHeader) {
  return ntohs(dnsHeader.QDCount) == 1 && dnsHeader.ANCount == 0 && dnsHeader.NSCount == 0 && dnsHeader.ARCount == 0;
}


static void dnss_receive_udp_packet_handler(
  
        void *arg,
        struct udp_pcb *udp_pcb,
        struct pbuf *udp_packet_buffer,
        struct ip_addr *sender_addr,
        uint16_t sender_port) 
{     
        /* To avoid gcc warnings */
        ( void ) arg;
        DNSHeader dnsHeader;
        DNSQuestion dnsQuestion;
        int sizeUrl;

        memcpy(&dnsHeader, udp_packet_buffer->payload, DNS_HEADER_SIZE);
        if (dnsHeader.QR != DNS_QR_QUERY) {
          return;  // ignore non-query messages
        }

        
        if (requestIncludesOnlyOneQuestion(dnsHeader)) {
            /*
              // The QName has a variable length, maximum 255 bytes and is comprised of multiple labels.
              // Each label contains a byte to describe its length and the label itself. The list of
              // labels terminates with a zero-valued byte. In "github.com", we have two labels "github" & "com"
        */
            const char *enoflbls = strchr(reinterpret_cast<const char *>(udp_packet_buffer->payload) + DNS_HEADER_SIZE, 0);  // find end_of_label marker
            ++enoflbls;                                                                                      // advance after null terminator
            dnsQuestion.QName = (uint8_t *)udp_packet_buffer->payload + DNS_HEADER_SIZE;                                                // we can reference labels from the request
            dnsQuestion.QNameLength = enoflbls - (char *)udp_packet_buffer->payload - DNS_HEADER_SIZE;
            sizeUrl = static_cast<int>(dnsQuestion.QNameLength);
            
            /*
                check if we aint going out of pkt bounds
                proper dns req should have label terminator at least 4 bytes before end of packet
              */
            if (dnsQuestion.QNameLength > udp_packet_buffer->len - DNS_HEADER_SIZE - sizeof(dnsQuestion.QType) - sizeof(dnsQuestion.QClass)) {
              return;  // malformed packet
            }         
            memcpy(&dnsQuestion.QType, enoflbls, sizeof(dnsQuestion.QType));
            memcpy(&dnsQuestion.QClass, enoflbls + sizeof(dnsQuestion.QType), sizeof(dnsQuestion.QClass));
        }
        if (dnsHeader.OPCode == DNS_OPCODE_QUERY && requestIncludesOnlyOneQuestion(dnsHeader)){
            struct dns_hdr *q = (struct dns_hdr*) udp_packet_buffer->payload;
                

            int questionSectionSize = dnsQuestion.QNameLength + sizeof(uint16_t) + sizeof(uint16_t);  // QName + QType + QClass

            // Reservamos espacio para el DNS response (Header + Question + Answer)
            struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, sizeof(struct dns_hdr) + questionSectionSize + 16, PBUF_RAM);

            if (p) {
                struct dns_hdr *rsp_hdr = (struct dns_hdr *)p->payload;
                struct dns_hdr *req_hdr = (struct dns_hdr *)udp_packet_buffer->payload;

                // Copiamos campos del header DNS
                rsp_hdr->id = req_hdr->id;
                rsp_hdr->flags1 = 0x85; // QR=1, AA=1, RD=1
                rsp_hdr->flags2 = 0x80; // RA=1
                rsp_hdr->numquestions = PP_HTONS(1);
                rsp_hdr->numanswers = PP_HTONS(1);
                rsp_hdr->numauthrr = PP_HTONS(0);
                rsp_hdr->numextrarr = PP_HTONS(0);

                // Copiamos la sección de pregunta desde el paquete original
                memcpy((uint8_t *)rsp_hdr + sizeof(struct dns_hdr),
                      (uint8_t *)udp_packet_buffer->payload + sizeof(struct dns_hdr),
                      questionSectionSize);

                // Construcción de la sección de respuesta
                uint8_t *answer_ptr = (uint8_t *)rsp_hdr + sizeof(struct dns_hdr) + questionSectionSize;

                // Nombre del dominio (compresión, apuntando a QName en offset 12)
                answer_ptr[0] = 0xC0;
                answer_ptr[1] = 0x0C;

                // Tipo A
                *(uint16_t *)(answer_ptr + 2) = PP_HTONS(1);

                // Clase IN
                *(uint16_t *)(answer_ptr + 4) = PP_HTONS(1);

                // TTL (0 segundos)
                *(uint32_t *)(answer_ptr + 6) = PP_HTONL(0);

                // Longitud de datos: 4 bytes (IPv4)
                *(uint16_t *)(answer_ptr + 10) = PP_HTONS(4);

                // Dirección IP a retornar (ej. dhcpServer contiene el puntero al uint8_t[4])
                memcpy(answer_ptr + 12, (void *)dhcpServer, 4);

                // Enviar respuesta
                udp_sendto(udp_pcb, p, sender_addr, sender_port);
                
            }
            pbuf_free(p);
        }
        //}       
        else {
                struct dns_hdr *dns_rsp;
                dns_rsp = (struct dns_hdr*) udp_packet_buffer->payload;

                dns_rsp->flags1 |= 0x80; // 0x80 : Response;
                dns_rsp->flags2 = 0x05;  //0x05 : Reply code (Query Refused)

                udp_sendto(udp_pcb, udp_packet_buffer, sender_addr, sender_port);
        }

        /* free the UDP connection, so we can accept new clients */
        udp_disconnect(udp_pcb);

        /* Free the packet buffer */
        pbuf_free(udp_packet_buffer);
}
static struct udp_pcb *dns_server_pcb;

void unbind_all_udp(){
  struct udp_pcb *pcb;
  for (pcb = udp_pcbs; pcb != NULL; pcb = pcb->next) {
      udp_remove(pcb);
  }
}

void unbind_dns(){
  //Buscamos el puerto udp de las dns para matarloo
  struct udp_pcb *pcb;
  for (pcb = udp_pcbs; pcb != NULL; pcb = pcb->next) {
    
    if(pcb->local_port==DNS_SERVER_PORT){
      udp_remove(pcb);
    }
  }
}

void start_DNS_Server(){
  dns_server_pcb = udp_new();
  udp_bind(dns_server_pcb, IP4_ADDR_ANY, DNS_SERVER_PORT);
  udp_recv(dns_server_pcb, (udp_recv_fn)dnss_receive_udp_packet_handler, NULL);
}
