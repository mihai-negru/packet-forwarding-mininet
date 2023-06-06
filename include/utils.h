#ifndef UTILS_H_
#define UTILS_H_

#include <arpa/inet.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "queue.h"
#include "lib.h"
#include "protocols.h"
#include "binary_trie.h"
#include "vector.h"

#define IP_TYPE htons(0x0800)
#define ARP_TYPE htons(0x0806)

#define OP_REQUEST htons(1)
#define OP_REPLAY htons(2)
#define HTYPE_ETHER htons(1)
#define HW_LEN (uint8_t)6
#define PT_LEN (uint8_t)4

#define ICMP_RESPONE (uint8_t)0
#define ICMP_TIME_EXCED (uint8_t)11
#define ICMP_DEST_UNREACH (uint8_t)3

typedef struct packed_msg_s {
	char *buf;
	size_t len;
	int interface;
	uint32_t hop;
} packed_msg_t;

typedef struct router_s {
	btrie_t *routes;						/* The Trie structure storing the routing table */
	vector_t *macs;							/* Cache memory in order to store the MAC addressed from ARP Replay */
	queue pckg_queue;						/* A queue with packets that are waiting for an ARP Replay */
	queue pckg_aux;							/* A queue that helps forwarding the packets that got the MAC address */

	struct ether_header *eth_hdr;			/* The Ethernet Header that coresponds to the current sending packet */
	struct iphdr *ip_hdr;					/* The IP Header that coresponds to the current sending packet (Optional) */
	struct arp_header *arp_hdr;				/* The ARP Header that coresponds to the current sending packet (Optional) */
	struct icmphdr *icmp_hdr;				/* The ICMP Header that coresponds to the current sending packet (Optional) */

	char buf[MAX_PACKET_LEN];				/* The current packet buffer */
	size_t len;								/* Length of the buffer that was read from the network */

	void (*ipv4)(struct router_s *this);	/* The handler function for IPv4 packets */
	void (*arp)(struct router_s *this);		/* The handler function for ARP packets */

	uint32_t next_hop;						/* The best hop chosen for the forwarding the packet */
	int interface;							/* The interface that the packet was received or the interface that the packet will be sent */
} router_t;

#define DEBUG(MSG) \
	do { \
		fprintf(stderr, "%s\n", MSG); \
	} while (0)

router_t* 	init_router			(char *path);
void 		free_router			(router_t *router);

uint8_t 	packet_is_ipv4		(router_t *router);
uint8_t 	packet_is_arp		(router_t *router);

int 		recv_msg			(router_t *router);
void 		init_msg_fields		(router_t *router);

#endif /* UTILS_H_ */