/*
 * VibeOS Network Stack
 *
 * Ethernet, ARP, IP, ICMP handling
 */

#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <stddef.h>

// Ethernet frame header
typedef struct __attribute__((packed)) {
    uint8_t dst[6];
    uint8_t src[6];
    uint16_t ethertype;
} eth_header_t;

// Ethertypes (big-endian on wire)
#define ETH_TYPE_IP   0x0800
#define ETH_TYPE_ARP  0x0806

// ARP packet
typedef struct __attribute__((packed)) {
    uint16_t htype;      // Hardware type (1 = Ethernet)
    uint16_t ptype;      // Protocol type (0x0800 = IPv4)
    uint8_t hlen;        // Hardware address length (6 for Ethernet)
    uint8_t plen;        // Protocol address length (4 for IPv4)
    uint16_t oper;       // Operation (1 = request, 2 = reply)
    uint8_t sha[6];      // Sender hardware address
    uint8_t spa[4];      // Sender protocol address
    uint8_t tha[6];      // Target hardware address
    uint8_t tpa[4];      // Target protocol address
} arp_packet_t;

#define ARP_OP_REQUEST  1
#define ARP_OP_REPLY    2

// IP header
typedef struct __attribute__((packed)) {
    uint8_t version_ihl;    // Version (4 bits) + IHL (4 bits)
    uint8_t tos;            // Type of service
    uint16_t total_len;     // Total length
    uint16_t id;            // Identification
    uint16_t flags_frag;    // Flags (3 bits) + Fragment offset (13 bits)
    uint8_t ttl;            // Time to live
    uint8_t protocol;       // Protocol (1=ICMP, 6=TCP, 17=UDP)
    uint16_t checksum;      // Header checksum
    uint32_t src_ip;        // Source IP
    uint32_t dst_ip;        // Destination IP
} ip_header_t;

// IP protocols
#define IP_PROTO_ICMP  1
#define IP_PROTO_TCP   6
#define IP_PROTO_UDP   17

// ICMP header
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} icmp_header_t;

// ICMP types
#define ICMP_ECHO_REPLY    0
#define ICMP_ECHO_REQUEST  8

// ARP table entry
typedef struct {
    uint32_t ip;
    uint8_t mac[6];
    uint8_t valid;
    uint32_t timestamp;  // For expiry (future)
} arp_entry_t;

// Network configuration (QEMU user-mode defaults)
#define NET_IP          0x0a00020f  // 10.0.2.15
#define NET_GATEWAY     0x0a000202  // 10.0.2.2
#define NET_DNS         0x0a000203  // 10.0.2.3
#define NET_NETMASK     0xffffff00  // 255.255.255.0

// Initialize network stack
void net_init(void);

// Process incoming packets (call from main loop or IRQ)
void net_poll(void);

// Send raw ethernet frame
int eth_send(const uint8_t *dst_mac, uint16_t ethertype, const void *data, uint32_t len);

// ARP functions
void arp_request(uint32_t ip);
const uint8_t *arp_lookup(uint32_t ip);

// IP functions
int ip_send(uint32_t dst_ip, uint8_t protocol, const void *data, uint32_t len);
uint16_t ip_checksum(const void *data, uint32_t len);

// ICMP functions
int icmp_send_echo_request(uint32_t dst_ip, uint16_t id, uint16_t seq, const void *data, uint32_t len);

// Ping interface (blocking, with timeout)
// Returns round-trip time in ms, or -1 on timeout
int net_ping(uint32_t ip, uint16_t seq, uint32_t timeout_ms);

// Get our IP/MAC
uint32_t net_get_ip(void);
void net_get_mac(uint8_t *mac);

// Helper: convert IP to string (uses static buffer)
const char *ip_to_str(uint32_t ip);

// Helper: make IP from bytes
#define MAKE_IP(a,b,c,d) (((uint32_t)(a)<<24)|((uint32_t)(b)<<16)|((uint32_t)(c)<<8)|(uint32_t)(d))

#endif
