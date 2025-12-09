/*
 * VibeOS Network Stack
 *
 * Ethernet, ARP, IP, ICMP implementation
 */

#include "net.h"
#include "virtio_net.h"
#include "printf.h"
#include "string.h"

// Our MAC and IP
static uint8_t our_mac[6];
static uint32_t our_ip = NET_IP;

// ARP table
#define ARP_TABLE_SIZE 16
static arp_entry_t arp_table[ARP_TABLE_SIZE];

// Packet buffer
static uint8_t pkt_buf[1600];

// Broadcast MAC
static const uint8_t broadcast_mac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// Ping state (for tracking echo replies)
static volatile int ping_received = 0;
static volatile uint16_t ping_id = 0;
static volatile uint16_t ping_seq = 0;

// Byte order helpers (network = big endian)
static inline uint16_t htons(uint16_t x) {
    return (x >> 8) | (x << 8);
}

static inline uint16_t ntohs(uint16_t x) {
    return htons(x);
}

static inline uint32_t htonl(uint32_t x) {
    return ((x >> 24) & 0xff) |
           ((x >> 8) & 0xff00) |
           ((x << 8) & 0xff0000) |
           ((x << 24) & 0xff000000);
}

static inline uint32_t ntohl(uint32_t x) {
    return htonl(x);
}

// IP to string (static buffer - not thread safe, but we're single-threaded)
static char ip_str_buf[16];
const char *ip_to_str(uint32_t ip) {
    uint8_t *b = (uint8_t *)&ip;
    // IP is stored in network order (big endian), so first byte is most significant
    int len = 0;
    for (int i = 0; i < 4; i++) {
        int val = b[3-i];  // Reverse for our MAKE_IP macro (host order)
        if (val >= 100) {
            ip_str_buf[len++] = '0' + val / 100;
            ip_str_buf[len++] = '0' + (val / 10) % 10;
            ip_str_buf[len++] = '0' + val % 10;
        } else if (val >= 10) {
            ip_str_buf[len++] = '0' + val / 10;
            ip_str_buf[len++] = '0' + val % 10;
        } else {
            ip_str_buf[len++] = '0' + val;
        }
        if (i < 3) ip_str_buf[len++] = '.';
    }
    ip_str_buf[len] = '\0';
    return ip_str_buf;
}

void net_init(void) {
    // Get our MAC from the driver
    virtio_net_get_mac(our_mac);

    // Clear ARP table
    memset(arp_table, 0, sizeof(arp_table));

    printf("[NET] Stack initialized, IP=%s\n", ip_to_str(our_ip));
}

uint32_t net_get_ip(void) {
    return our_ip;
}

void net_get_mac(uint8_t *mac) {
    memcpy(mac, our_mac, 6);
}

// Send ethernet frame
int eth_send(const uint8_t *dst_mac, uint16_t ethertype, const void *data, uint32_t len) {
    if (len > NET_MTU - sizeof(eth_header_t)) {
        return -1;
    }

    // Build frame
    eth_header_t *eth = (eth_header_t *)pkt_buf;
    memcpy(eth->dst, dst_mac, 6);
    memcpy(eth->src, our_mac, 6);
    eth->ethertype = htons(ethertype);

    memcpy(pkt_buf + sizeof(eth_header_t), data, len);

    return virtio_net_send(pkt_buf, sizeof(eth_header_t) + len);
}

// ARP table lookup
const uint8_t *arp_lookup(uint32_t ip) {
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            return arp_table[i].mac;
        }
    }
    return NULL;
}

// Add/update ARP entry
static void arp_add(uint32_t ip, const uint8_t *mac) {
    // Check if already exists
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            memcpy(arp_table[i].mac, mac, 6);
            return;
        }
    }

    // Find free slot
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (!arp_table[i].valid) {
            arp_table[i].ip = ip;
            memcpy(arp_table[i].mac, mac, 6);
            arp_table[i].valid = 1;
            printf("[ARP] Added %s -> %02x:%02x:%02x:%02x:%02x:%02x\n",
                   ip_to_str(ip), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            return;
        }
    }

    // Table full - overwrite first entry (simple LRU)
    arp_table[0].ip = ip;
    memcpy(arp_table[0].mac, mac, 6);
    arp_table[0].valid = 1;
}

// Send ARP request
void arp_request(uint32_t ip) {
    arp_packet_t arp;
    arp.htype = htons(1);        // Ethernet
    arp.ptype = htons(0x0800);   // IPv4
    arp.hlen = 6;
    arp.plen = 4;
    arp.oper = htons(ARP_OP_REQUEST);
    memcpy(arp.sha, our_mac, 6);

    // Convert our IP to network byte order
    uint32_t our_ip_net = htonl(our_ip);
    memcpy(arp.spa, &our_ip_net, 4);

    memset(arp.tha, 0, 6);       // Unknown
    uint32_t ip_net = htonl(ip);
    memcpy(arp.tpa, &ip_net, 4);

    printf("[ARP] Requesting %s\n", ip_to_str(ip));
    eth_send(broadcast_mac, ETH_TYPE_ARP, &arp, sizeof(arp));
}

// Handle incoming ARP packet
static void arp_handle(const uint8_t *pkt, uint32_t len) {
    if (len < sizeof(arp_packet_t)) return;

    const arp_packet_t *arp = (const arp_packet_t *)pkt;

    // Only handle Ethernet/IPv4
    if (ntohs(arp->htype) != 1 || ntohs(arp->ptype) != 0x0800) return;
    if (arp->hlen != 6 || arp->plen != 4) return;

    uint32_t sender_ip, target_ip;
    memcpy(&sender_ip, arp->spa, 4);
    memcpy(&target_ip, arp->tpa, 4);
    sender_ip = ntohl(sender_ip);
    target_ip = ntohl(target_ip);

    uint16_t op = ntohs(arp->oper);

    // Learn sender's MAC (even if not for us)
    arp_add(sender_ip, arp->sha);

    if (op == ARP_OP_REQUEST) {
        // Is this asking for our MAC?
        if (target_ip == our_ip) {
            printf("[ARP] Request for our IP from %s\n", ip_to_str(sender_ip));

            // Send reply
            arp_packet_t reply;
            reply.htype = htons(1);
            reply.ptype = htons(0x0800);
            reply.hlen = 6;
            reply.plen = 4;
            reply.oper = htons(ARP_OP_REPLY);
            memcpy(reply.sha, our_mac, 6);
            uint32_t our_ip_net = htonl(our_ip);
            memcpy(reply.spa, &our_ip_net, 4);
            memcpy(reply.tha, arp->sha, 6);
            memcpy(reply.tpa, arp->spa, 4);

            eth_send(arp->sha, ETH_TYPE_ARP, &reply, sizeof(reply));
            printf("[ARP] Sent reply\n");
        }
    } else if (op == ARP_OP_REPLY) {
        printf("[ARP] Reply from %s\n", ip_to_str(sender_ip));
        // Already added to table above
    }
}

// IP checksum
uint16_t ip_checksum(const void *data, uint32_t len) {
    const uint16_t *ptr = (const uint16_t *)data;
    uint32_t sum = 0;

    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }

    if (len == 1) {
        sum += *(const uint8_t *)ptr;
    }

    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }

    return ~sum;
}

// Handle incoming ICMP packet
static void icmp_handle(const uint8_t *pkt, uint32_t len, uint32_t src_ip) {
    if (len < sizeof(icmp_header_t)) return;

    const icmp_header_t *icmp = (const icmp_header_t *)pkt;

    if (icmp->type == ICMP_ECHO_REQUEST) {
        printf("[ICMP] Echo request from %s\n", ip_to_str(src_ip));

        // Send echo reply
        uint8_t reply_buf[1500];
        icmp_header_t *reply = (icmp_header_t *)reply_buf;

        reply->type = ICMP_ECHO_REPLY;
        reply->code = 0;
        reply->checksum = 0;
        reply->id = icmp->id;
        reply->seq = icmp->seq;

        // Copy data portion
        uint32_t data_len = len - sizeof(icmp_header_t);
        if (data_len > sizeof(reply_buf) - sizeof(icmp_header_t)) {
            data_len = sizeof(reply_buf) - sizeof(icmp_header_t);
        }
        memcpy(reply_buf + sizeof(icmp_header_t), pkt + sizeof(icmp_header_t), data_len);

        // Calculate checksum
        reply->checksum = ip_checksum(reply_buf, sizeof(icmp_header_t) + data_len);

        ip_send(src_ip, IP_PROTO_ICMP, reply_buf, sizeof(icmp_header_t) + data_len);
        printf("[ICMP] Sent echo reply\n");
    }
    else if (icmp->type == ICMP_ECHO_REPLY) {
        printf("[ICMP] Echo reply from %s id=%d seq=%d\n",
               ip_to_str(src_ip), ntohs(icmp->id), ntohs(icmp->seq));

        // Check if this matches our pending ping
        if (ntohs(icmp->id) == ping_id && ntohs(icmp->seq) == ping_seq) {
            ping_received = 1;
        }
    }
}

// Handle incoming IP packet
static void ip_handle(const uint8_t *pkt, uint32_t len) {
    if (len < sizeof(ip_header_t)) return;

    const ip_header_t *ip = (const ip_header_t *)pkt;

    // Check version
    if ((ip->version_ihl >> 4) != 4) return;

    // Get header length
    uint32_t ihl = (ip->version_ihl & 0x0f) * 4;
    if (ihl < 20 || ihl > len) return;

    // Check if it's for us
    uint32_t dst_ip = ntohl(ip->dst_ip);
    if (dst_ip != our_ip && dst_ip != 0xffffffff) return;

    uint32_t src_ip = ntohl(ip->src_ip);
    uint32_t payload_len = ntohs(ip->total_len) - ihl;
    const uint8_t *payload = pkt + ihl;

    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            icmp_handle(payload, payload_len, src_ip);
            break;
        case IP_PROTO_UDP:
            // TODO: UDP handling
            printf("[IP] UDP packet from %s (not implemented)\n", ip_to_str(src_ip));
            break;
        case IP_PROTO_TCP:
            // TODO: TCP handling
            printf("[IP] TCP packet from %s (not implemented)\n", ip_to_str(src_ip));
            break;
        default:
            printf("[IP] Unknown protocol %d from %s\n", ip->protocol, ip_to_str(src_ip));
            break;
    }
}

// Send IP packet
int ip_send(uint32_t dst_ip, uint8_t protocol, const void *data, uint32_t len) {
    if (len > NET_MTU - sizeof(eth_header_t) - sizeof(ip_header_t)) {
        return -1;
    }

    // Determine next hop MAC
    const uint8_t *dst_mac;
    uint32_t next_hop = dst_ip;

    // If destination is not on local network, use gateway
    if ((dst_ip & NET_NETMASK) != (our_ip & NET_NETMASK)) {
        next_hop = NET_GATEWAY;
    }

    dst_mac = arp_lookup(next_hop);
    if (!dst_mac) {
        // Need to ARP first
        printf("[IP] No ARP entry for %s, sending request\n", ip_to_str(next_hop));
        arp_request(next_hop);
        return -1;  // Caller should retry
    }

    // Build IP packet
    static uint8_t ip_buf[1600];
    ip_header_t *ip = (ip_header_t *)ip_buf;

    ip->version_ihl = 0x45;  // IPv4, 20 byte header
    ip->tos = 0;
    ip->total_len = htons(sizeof(ip_header_t) + len);
    ip->id = htons(0);  // TODO: track ID
    ip->flags_frag = 0;
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_ip = htonl(our_ip);
    ip->dst_ip = htonl(dst_ip);

    // Calculate header checksum
    ip->checksum = ip_checksum(ip, sizeof(ip_header_t));

    // Copy payload
    memcpy(ip_buf + sizeof(ip_header_t), data, len);

    return eth_send(dst_mac, ETH_TYPE_IP, ip_buf, sizeof(ip_header_t) + len);
}

// Send ICMP echo request
int icmp_send_echo_request(uint32_t dst_ip, uint16_t id, uint16_t seq, const void *data, uint32_t len) {
    uint8_t icmp_buf[1500];
    icmp_header_t *icmp = (icmp_header_t *)icmp_buf;

    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(id);
    icmp->seq = htons(seq);

    // Copy data
    if (len > sizeof(icmp_buf) - sizeof(icmp_header_t)) {
        len = sizeof(icmp_buf) - sizeof(icmp_header_t);
    }
    if (data && len > 0) {
        memcpy(icmp_buf + sizeof(icmp_header_t), data, len);
    }

    // Calculate checksum
    icmp->checksum = ip_checksum(icmp_buf, sizeof(icmp_header_t) + len);

    return ip_send(dst_ip, IP_PROTO_ICMP, icmp_buf, sizeof(icmp_header_t) + len);
}

// Process incoming packets
void net_poll(void) {
    static uint8_t rx_buf[1600];

    while (virtio_net_has_packet()) {
        int len = virtio_net_recv(rx_buf, sizeof(rx_buf));
        if (len <= 0) break;

        if (len < (int)sizeof(eth_header_t)) continue;

        eth_header_t *eth = (eth_header_t *)rx_buf;
        uint16_t ethertype = ntohs(eth->ethertype);

        const uint8_t *payload = rx_buf + sizeof(eth_header_t);
        uint32_t payload_len = len - sizeof(eth_header_t);

        switch (ethertype) {
            case ETH_TYPE_ARP:
                arp_handle(payload, payload_len);
                break;
            case ETH_TYPE_IP:
                ip_handle(payload, payload_len);
                break;
            default:
                // Ignore unknown ethertypes
                break;
        }
    }
}

// Blocking ping with timeout
int net_ping(uint32_t ip, uint16_t seq, uint32_t timeout_ms) {
    // First, make sure we have ARP entry for the target (or gateway)
    uint32_t next_hop = ip;
    if ((ip & NET_NETMASK) != (our_ip & NET_NETMASK)) {
        next_hop = NET_GATEWAY;
    }

    // Try to get ARP entry, send request if needed
    if (!arp_lookup(next_hop)) {
        arp_request(next_hop);

        // Wait for ARP reply (up to 1 second)
        for (int i = 0; i < 100 && !arp_lookup(next_hop); i++) {
            net_poll();
            // Simple delay (~10ms)
            for (volatile int j = 0; j < 100000; j++);
        }

        if (!arp_lookup(next_hop)) {
            printf("[PING] ARP timeout for %s\n", ip_to_str(next_hop));
            return -1;
        }
    }

    // Set up ping tracking
    ping_id = 0x1234;
    ping_seq = seq;
    ping_received = 0;

    // Send echo request
    uint8_t ping_data[56];
    memset(ping_data, 0xAB, sizeof(ping_data));

    if (icmp_send_echo_request(ip, ping_id, seq, ping_data, sizeof(ping_data)) < 0) {
        return -1;
    }

    // Wait for reply
    for (uint32_t i = 0; i < timeout_ms / 10 && !ping_received; i++) {
        net_poll();
        // Simple delay (~10ms)
        for (volatile int j = 0; j < 100000; j++);
    }

    if (ping_received) {
        return 0;  // TODO: return actual RTT
    }

    return -1;  // Timeout
}
