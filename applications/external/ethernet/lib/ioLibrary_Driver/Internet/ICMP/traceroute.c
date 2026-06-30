#include "traceroute.h"
#include "ping.h"
#include <string.h>

static uint16_t RandomID = 0x1234;
static uint16_t RandomSeqNum = 0x4321;

uint16_t port = PORT; // Store the port number in a variable

uint8_t traceroute(uint8_t s, uint8_t *dest_addr)
{
    uint8_t ttl = 1;
    int ret;

    while (ttl <= MAX_HOPS)
    {
        uint8_t sr = getSn_SR(s);
        eth_printf("SR: %02X", sr);

        switch (sr)
        {
        case SOCK_CLOSED:
            close(s);
            IINCHIP_WRITE(Sn_PROTO(s), IPPROTO_ICMP); // Set ICMP Protocol
            if ((ret = socket(s, Sn_MR_IPRAW, 3000, 0)) != s)
            {
                eth_printf("Socket %d failed: %d", s, ret);
                return SOCKET_ERROR;
            }
            while (getSn_SR(s) != SOCK_IPRAW)
                ;
            ping_wait_ms(500);
            break;

        case SOCK_IPRAW:
            send_traceroute_request(s, dest_addr, ttl);

            for (int i = 0; i < 40; i++)
            { // Wait up to 2 seconds
                uint16_t len = getSn_RX_RSR(s);
                if (len > 0)
                {
                    uint8_t recv_addr[4];
                    receive_traceroute_reply(s, recv_addr, len);

                    eth_printf("Hop %d: %d.%d.%d.%d", ttl, recv_addr[0], recv_addr[1], recv_addr[2], recv_addr[3]);
                    if (memcmp(recv_addr, dest_addr, 4) == 0)
                    {
                        eth_printf("Destination reached: %d.%d.%d.%d", recv_addr[0], recv_addr[1], recv_addr[2], recv_addr[3]);
                        return 0; // Destination reached
                    }
                    break;
                }
                ping_wait_ms(50);
            }
            break;

        default:
            break;
        }

        ttl++;
        if (ttl > MAX_HOPS)
        {
            eth_printf("Max hops reached.");
            break;
        }
    }
    return FUNCTION_ERROR;
}

void send_traceroute_request(uint8_t s, uint8_t *dest_addr, uint8_t ttl)
{
    PINGMSGR PingRequest;

    PingRequest.Type = PING_REQUEST;
    PingRequest.Code = CODE_ZERO;
    PingRequest.ID = htons(RandomID++);
    PingRequest.SeqNum = htons(RandomSeqNum++);

    for (int i = 0; i < BUF_LEN; i++)
    {
        PingRequest.Data[i] = (i) % 8;
    }

    PingRequest.CheckSum = 0;
    PingRequest.CheckSum = htons(checksum((uint8_t *)&PingRequest, sizeof(PingRequest)));

    set_ttl(s, ttl);                                                          // Set TTL
    sendto(s, (uint8_t *)&PingRequest, sizeof(PingRequest), dest_addr, port); // Use the port variable

    eth_printf("Sent traceroute request (TTL: %d) to %d.%d.%d.%d", ttl, dest_addr[0], dest_addr[1], dest_addr[2], dest_addr[3]);
}

void receive_traceroute_reply(uint8_t s, uint8_t *addr, uint16_t len)
{
    uint8_t data_buf[128];

    uint16_t rlen = recvfrom(s, data_buf, len, addr, &port); // Pass the pointer to the port variable
    (void)rlen;
    if (data_buf[0] == PING_REPLY)
    {
        eth_printf("Reply from %d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
    }
    else if (data_buf[0] == 11)
    { // ICMP Time Exceeded
        eth_printf("Time Exceeded from %d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
    }
}

void set_ttl(uint8_t s, uint8_t ttl)
{
    IINCHIP_WRITE(Sn_TTL(s), ttl);
}
