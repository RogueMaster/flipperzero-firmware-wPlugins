/*
 * Symbol table of the LAN Tester app's private functions exposed to plugins.
 * Grows as more categories are converted. Declarations come from the
 * C-linkage exports header + ioLibrary shim (no C++ templates pulled in).
 */
#include "lan_tester_exports.h"

static constexpr auto lan_tester_api_table = sort(create_array_t<sym_entry>(
    // app helpers
    API_METHOD(lan_tester_ensure_w5500, bool, (LanTesterApp*)),
    API_METHOD(lan_tester_check_w5500, bool, (LanTesterApp*)),
    API_METHOD(lan_tester_update_view, void, (TextBox*, FuriString*)),
    API_METHOD(lan_tester_count_frame, void, (LanTesterApp*, const uint8_t*, uint16_t)),
    API_METHOD(lan_tester_save_and_notify, void, (LanTesterApp*, const char*, FuriString*)),
    // W5500 HAL
    API_METHOD(w5500_hal_get_phy_info, void, (bool*, uint8_t*, uint8_t*)),
    API_METHOD(w5500_hal_get_mac, void, (uint8_t*)),
    API_METHOD(w5500_hal_get_link_status, bool, (void)),
    API_METHOD(w5500_hal_open_macraw, bool, (void)),
    API_METHOD(w5500_hal_close_macraw, void, (void)),
    API_METHOD(w5500_hal_macraw_recv, uint16_t, (uint8_t*, uint16_t)),
    // utils
    API_METHOD(pkt_get_ethertype, uint16_t, (const uint8_t*)),
    API_METHOD(pkt_get_dst_mac, void, (const uint8_t*, uint8_t*)),
    API_METHOD(pkt_get_src_mac, void, (const uint8_t*, uint8_t*)),
    API_METHOD(pkt_is_broadcast, bool, (const uint8_t*)),
    API_METHOD(pkt_is_multicast, bool, (const uint8_t*)),
    API_METHOD(pkt_format_mac, void, (const uint8_t*, char*)),
    API_METHOD(pkt_format_ip, void, (const uint8_t*, char*)),
    API_METHOD(pkt_checksum, uint16_t, (const uint8_t*, uint16_t)),
    API_METHOD(pkt_read_u16_be, uint16_t, (const uint8_t*)),
    API_METHOD(pkt_read_u32_be, uint32_t, (const uint8_t*)),
    API_METHOD(pkt_write_u16_be, void, (uint8_t*, uint16_t)),
    API_METHOD(pkt_write_u32_be, void, (uint8_t*, uint32_t)),
    API_METHOD(oui_lookup, const char*, (const uint8_t*)),
    // ioLibrary
    API_METHOD(socket, int8_t, (uint8_t, uint8_t, uint16_t, uint8_t)),
    API_METHOD(close, int8_t, (uint8_t)),
    API_METHOD(listen, int8_t, (uint8_t)),
    API_METHOD(connect, int8_t, (uint8_t, uint8_t*, uint16_t)),
    API_METHOD(disconnect, int8_t, (uint8_t)),
    API_METHOD(send, int32_t, (uint8_t, uint8_t*, uint16_t)),
    API_METHOD(recv, int32_t, (uint8_t, uint8_t*, uint16_t)),
    API_METHOD(sendto, int32_t, (uint8_t, uint8_t*, uint16_t, uint8_t*, uint16_t)),
    API_METHOD(recvfrom, int32_t, (uint8_t, uint8_t*, uint16_t, uint8_t*, uint16_t*)),
    API_METHOD(getSn_RX_RSR, uint16_t, (uint8_t)),
    API_METHOD(getSn_TX_FSR, uint16_t, (uint8_t)),
    API_METHOD(lt_getSn_IR, uint8_t, (uint8_t)),
    API_METHOD(lt_getSn_SR, uint8_t, (uint8_t)),
    API_METHOD(lt_getSn_TxMAX, uint16_t, (uint8_t)),
    API_METHOD(wizchip_getnetinfo, void, (wiz_NetInfo*)),
    API_METHOD(wizchip_setnetinfo, void, (wiz_NetInfo*)),
    // Scan category
    API_METHOD(w5500_hal_macraw_send, uint16_t, (const uint8_t*, uint16_t)),
    API_METHOD(lan_tester_check_dhcp, bool, (LanTesterApp*)),
    API_METHOD(lan_tester_ensure_dhcp, bool, (LanTesterApp*)),
    API_METHOD(lan_tester_save_results, bool, (const char*, const char*)),
    API_METHOD(lan_tester_progress_bar, void, (char*, uint8_t, uint16_t, uint16_t)),
    API_METHOD(scan_results_clear, void, (void)),
    API_METHOD(scan_results_open_writer, bool, (void)),
    API_METHOD(scan_results_close_writer, void, (void)),
    API_METHOD(scan_results_add, void, (const uint8_t*, const uint8_t*)),
    API_METHOD(parse_cidr, bool, (const char*, uint8_t*, uint8_t*)),
    API_METHOD(
        arp_build_request,
        uint16_t,
        (uint8_t*, const uint8_t*, const uint8_t*, const uint8_t*)),
    API_METHOD(arp_parse_reply, bool, (const uint8_t*, uint16_t, uint8_t*, uint8_t*)),
    API_METHOD(arp_calc_scan_range, uint16_t, (const uint8_t*, const uint8_t*, uint8_t*, uint8_t*)),
    API_METHOD(arp_mask_to_prefix, uint8_t, (const uint8_t*)),
    API_METHOD(
        icmp_ping,
        bool,
        (uint8_t, const uint8_t*, uint16_t, uint32_t, PingResult*, const volatile bool*)),
    API_METHOD(port_scan_tcp, int, (uint8_t, const uint8_t*, uint16_t, uint32_t)),
    API_VARIABLE(PORT_PRESET_TOP20, uint16_t[]),
    API_VARIABLE(PORT_PRESET_TOP100, uint16_t[]),
    // Diagnostics category
    API_METHOD(lan_tester_get_dns_server, void, (LanTesterApp*, uint8_t*)),
    API_METHOD(dns_lookup, bool, (uint8_t, const uint8_t*, const char*, DnsLookupResult*)),
    API_METHOD(
        traceroute_send_hop,
        bool,
        (uint8_t, const uint8_t*, uint8_t, uint16_t, uint32_t, TracerouteHop*)),
    // Utilities category
    API_METHOD(lldp_parse, bool, (const uint8_t*, uint16_t, LldpNeighbor*)),
    API_METHOD(cdp_parse, bool, (const uint8_t*, uint16_t, CdpNeighbor*)),
    API_METHOD(cdp_check_frame, uint16_t, (const uint8_t*, uint16_t)),
    API_METHOD(
        w5500_hal_set_net_info,
        void,
        (const uint8_t*, const uint8_t*, const uint8_t*, const uint8_t*))));
