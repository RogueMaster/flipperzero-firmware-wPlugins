#pragma once

/*
 * Minimal replication of the private ioLibrary API that category plugins call.
 * Plugins cannot include the app's private ioLibrary headers (fbt only adds
 * those include paths to the private-lib build), so the small subset they use
 * is declared here and resolved at load via the host's API table. Layouts and
 * signatures MUST match lib/ioLibrary_Driver exactly.
 *
 * Included by the plugins (in place of socket.h/wizchip_conf.h) and by the
 * host's API table.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ethernet/wizchip_conf.h — W5500 wiz_NetInfo layout */
typedef enum {
    NETINFO_STATIC = 1,
    NETINFO_DHCP,
} dhcp_mode;

typedef struct {
    uint8_t mac[6];
    uint8_t ip[4];
    uint8_t sn[4];
    uint8_t gw[4];
    uint8_t dns[4];
    dhcp_mode dhcp;
} wiz_NetInfo;

/* W5500/w5500.h + Ethernet/socket.h constants (values copied verbatim from
 * lib/ioLibrary_Driver — must stay in sync with the vendored headers). */
/* Sn_MR socket modes (w5500.h) */
#define Sn_MR_CLOSE      0x00
#define Sn_MR_TCP        0x01
#define Sn_MR_UDP        0x02
/* Sn_IR interrupt flags (w5500.h) */
#define Sn_IR_CON        0x01
#define Sn_IR_DISCON     0x02
#define Sn_IR_RECV       0x04
#define Sn_IR_TIMEOUT    0x08
#define Sn_IR_SENDOK     0x10
/* Sn_SR socket status (w5500.h) */
#define SOCK_CLOSED      0x00
#define SOCK_INIT        0x13
#define SOCK_LISTEN      0x14
#define SOCK_ESTABLISHED 0x17
#define SOCK_CLOSE_WAIT  0x1C
#define SOCK_UDP         0x22
/* socket() family return codes (socket.h) */
#define SOCK_OK          1
#define SOCK_BUSY        0
#define SOCK_FATAL       (-1000)

/* Ethernet/socket.h */
int8_t socket(uint8_t sn, uint8_t protocol, uint16_t port, uint8_t flag);
int8_t close(uint8_t sn);
int8_t listen(uint8_t sn);
int8_t connect(uint8_t sn, uint8_t* addr, uint16_t port);
int8_t disconnect(uint8_t sn);
int32_t send(uint8_t sn, uint8_t* buf, uint16_t len);
int32_t recv(uint8_t sn, uint8_t* buf, uint16_t len);
int32_t sendto(uint8_t sn, uint8_t* buf, uint16_t len, uint8_t* addr, uint16_t port);
int32_t recvfrom(uint8_t sn, uint8_t* buf, uint16_t len, uint8_t* addr, uint16_t* port);

/* W5500/w5500.h — real-function register accessors */
uint16_t getSn_RX_RSR(uint8_t sn);
uint16_t getSn_TX_FSR(uint8_t sn);

/* W5500/w5500.h — register accessors that are macros over WIZCHIP_READ in the
 * vendored header; a macro cannot cross the plugin boundary, so the host wraps
 * each in a real function (lan_tester_ioshim_host.c) and the plugin-side macro
 * name maps to that wrapper. */
uint8_t lt_getSn_IR(uint8_t sn);
uint8_t lt_getSn_SR(uint8_t sn);
uint16_t lt_getSn_TxMAX(uint8_t sn);
#define getSn_IR(sn)    lt_getSn_IR(sn)
#define getSn_SR(sn)    lt_getSn_SR(sn)
#define getSn_TxMAX(sn) lt_getSn_TxMAX(sn)

/* Ethernet/wizchip_conf.h */
void wizchip_getnetinfo(wiz_NetInfo* pnetinfo);
void wizchip_setnetinfo(wiz_NetInfo* pnetinfo);

#ifdef __cplusplus
}
#endif
