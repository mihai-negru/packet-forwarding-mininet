#include "utils.h"

static void generate_arp_request(router_t *this) {
	this->arp_hdr = (struct arp_header *)(this->buf + sizeof *this->eth_hdr);

	this->eth_hdr->ether_type = ARP_TYPE;

	/* Initiate a broadcast packet */
	get_interface_mac(this->interface, this->eth_hdr->ether_shost);
	memset(this->eth_hdr->ether_dhost, 0xff, MAC_ADDR_SIZE);

	/* Initiate the basic ARP Request flags */
	this->arp_hdr->htype = HTYPE_ETHER;
	this->arp_hdr->ptype = IP_TYPE;
	this->arp_hdr->hlen = HW_LEN;
	this->arp_hdr->plen = PT_LEN;
	this->arp_hdr->op = OP_REQUEST;

	/* Set the source and the target ip addresses and mac addresses (0 for target) */

	get_interface_mac(this->interface, this->arp_hdr->sha);
	this->arp_hdr->spa = get_interface_ipv4(this->interface);

	memset(this->arp_hdr->tha, 0, MAC_ADDR_SIZE);
	this->arp_hdr->tpa = this->next_hop;

	/* Update the packet length for sending */
	this->len = sizeof(*this->eth_hdr) + sizeof(*this->arp_hdr);
}

static void generate_ipv4_header(router_t *this, uint8_t type) {

	/* Set the fields */
	this->ip_hdr->ihl = 5;
	this->ip_hdr->version = 4;
	this->ip_hdr->tos = 0;

	/* Compute the total length as one ipv4 header and one icmp */
	this->ip_hdr->tot_len = sizeof *this->ip_hdr + sizeof *this->icmp_hdr;
	if ((type == ICMP_TIME_EXCED) || (type == ICMP_DEST_UNREACH)) {

		/*
		 * If the replay is a time limit or host unreachable then
		 * we also send the original ipv4 header and 64 bits, so
		 * we add it to the total size
		*/
		this->ip_hdr->tot_len += sizeof *this->ip_hdr + 8;
	}
	this->ip_hdr->tot_len = htons(this->ip_hdr->tot_len);

	this->ip_hdr->id = htons(1);
	this->ip_hdr->frag_off = 0;
	this->ip_hdr->ttl = 64;
	this->ip_hdr->protocol = 1;
	this->ip_hdr->check = 0;
	this->ip_hdr->check = htons(checksum((uint16_t *)this->ip_hdr, sizeof *this->ip_hdr));

	/* Set the addresses in order to send the packet to the source */
	this->ip_hdr->daddr = this->ip_hdr->saddr;
	this->ip_hdr->saddr = get_interface_ipv4(this->interface);
}

static void generate_icmp_replay(router_t *this, uint8_t type) {
	this->icmp_hdr = (struct icmphdr *)(this->buf + sizeof *this->eth_hdr + sizeof *this->ip_hdr);

	this->len = sizeof *this->eth_hdr + sizeof *this->ip_hdr + sizeof *this->icmp_hdr;

	/* Copy the ipv4 header that generated an error in forwarding */
	if ((type == ICMP_TIME_EXCED) || (type == ICMP_DEST_UNREACH)) {
		memcpy(this->buf + this->len, this->ip_hdr, sizeof *this->ip_hdr);
		this->len += sizeof *this->ip_hdr;	

		memcpy(this->buf + this->len, this->ip_hdr + sizeof *this->ip_hdr, 8);
		this->len += 8;
	}

	/* Set the type and the code for the icmp replay */
	this->icmp_hdr->type = type;
	this->icmp_hdr->code = 0;

	/* Compute the checksum of the icmp header */
	this->icmp_hdr->checksum = 0;
	this->icmp_hdr->checksum = htons(checksum((uint16_t *)this->icmp_hdr, sizeof *this->icmp_hdr));

	/* Generate a new ipv4 header for the new packet */
	generate_ipv4_header(this, type);

	/* Update the ethernet header, to send the replay to the source */
	memcpy(this->eth_hdr->ether_dhost, this->eth_hdr->ether_shost, MAC_ADDR_SIZE);
	get_interface_mac(this->interface, this->eth_hdr->ether_shost);
}

static packed_msg_t* pack_the_msg(router_t *this) {
	if (this == NULL) {
		return NULL;
	}

	packed_msg_t *new_pckg = malloc(sizeof *new_pckg);

	if (new_pckg != NULL) {
		new_pckg->buf = malloc(MAX_PACKET_LEN);

		if (new_pckg->buf != NULL) {
			memcpy(new_pckg->buf, this->buf, this->len);

			new_pckg->len = this->len;
			new_pckg->interface = this->interface;
			new_pckg->hop = this->next_hop;
		} else {
			free(new_pckg);
			new_pckg = NULL;
		}
	}

	return new_pckg;
}

static void free_packed_msg(packed_msg_t *pckg) {
	if (pckg != NULL) {
		free(pckg->buf);
		free(pckg);
	}
}

static void ipv4_handler(router_t *this) {
	this->ip_hdr = (struct iphdr *)(this->buf + sizeof *this->eth_hdr);

    uint16_t old_check = this->ip_hdr->check;
	this->ip_hdr->check = 0;

	/* Check if the cheksum is good */
	if (old_check == htons(checksum((uint16_t *)this->ip_hdr, sizeof *this->ip_hdr))) {
		if (this->ip_hdr->daddr == get_interface_ipv4(this->interface)) {

			/* The packet was sent to this router so send a icmp replay */
			generate_icmp_replay(this, ICMP_RESPONE);
		} else {

			/* Compute the next hop via LPM */
			hop_info_t* best_route = btrie_lpm(this->routes, this->ip_hdr->daddr);

			if (best_route == NULL) {

				/*
				 * If the best route is NULL it means there is no way to send
				 * the packet so send back a icmp replay with host unreachable
				 */
				generate_icmp_replay(this, ICMP_DEST_UNREACH);
			} else {

				/* The next hop was found so we try to sent the packet */

				this->next_hop = best_route->hop;
				this->interface = best_route->interface;

				free(best_route);

				/* Check if the packet lived enough or not */
				if (this->ip_hdr->ttl > 1) {

					/* Compute the checksum after decrementing the time-to-live */
					this->ip_hdr->check = 0;
					this->ip_hdr->check = ~(~old_check + ~((uint16_t)this->ip_hdr->ttl) + (uint16_t)(this->ip_hdr->ttl - 1)) - 1;
					this->ip_hdr->ttl -= 1;

					/* Try to fetch the MAC address of the next hop */
					int entry_idx = get_mac_entry(this->macs, this->next_hop);

					if (entry_idx < 0) {

						/* The MAC address was not found so send an ARP Request */

						/* Pack the current message into the waiting queue */
						packed_msg_t *pckg = pack_the_msg(this);
						if (pckg != NULL) {
							queue_enq(this->pckg_queue, (void *)pckg);
						}

						/* Generate an arp request to the next hop to find MAC address */
						generate_arp_request(this);
					} else {

						/* The MAC address was found, update the ethernet header */
						memcpy(this->eth_hdr->ether_dhost, this->macs->addrs[entry_idx].mac, MAC_ADDR_SIZE);
						get_interface_mac(this->interface, this->eth_hdr->ether_shost);
					}
				} else {

					/* The packet lived enough, generate the time excedded icmp replay */
					generate_icmp_replay(this, ICMP_TIME_EXCED);
				}
			}
		}

		/*
		 * Send the modified packet to the next hop
		 * or to another interface for an
		 * ARP Request or ICMP Replay
		 */
		send_to_link(this->interface, this->buf, this->len);
	}
}

static void generate_arp_replay(router_t *this) {

	/* Set the ARP Op Code as a replay */
	this->arp_hdr->op = OP_REPLAY;

	/* Set the target as the source */
	this->arp_hdr->tpa = this->arp_hdr->spa;
	memcpy(this->arp_hdr->tha, this->arp_hdr->sha, MAC_ADDR_SIZE);

	/* Set the source as the target */
	get_interface_mac(this->interface, this->arp_hdr->sha);
	this->arp_hdr->spa = get_interface_ipv4(this->interface);

	/* Update the ethernet header to send the message back to the source */
	memcpy(this->eth_hdr->ether_dhost, this->eth_hdr->ether_shost, MAC_ADDR_SIZE);
	get_interface_mac(this->interface, this->eth_hdr->ether_shost);
}

static void process_waiting_packet(router_t *this, packed_msg_t *pckg) {

	/* Populate the packet data into the router fields */
	this->eth_hdr = (struct ether_header *)pckg->buf;
	this->ip_hdr = (struct iphdr *)(pckg->buf + sizeof *this->eth_hdr);

	this->len = pckg->len;
	this->interface = pckg->interface;
	this->next_hop = pckg->hop;

	/* Set the ethernet header type as an IPv4 packet */
	this->eth_hdr->ether_type = IP_TYPE;

	/* Update the ethernet header to send the packet to the next hop */
	memcpy(this->eth_hdr->ether_dhost, this->arp_hdr->sha, MAC_ADDR_SIZE);
	get_interface_mac(this->interface, this->eth_hdr->ether_shost);
}

static void reinit_the_waiting_queue(router_t *this) {

	/* Change the auxiliar queue with the waiting queue */
	queue temp = this->pckg_queue;
	this->pckg_queue = this->pckg_aux;
	this->pckg_aux = temp;
}

static void arp_handler(router_t *this) {
	this->arp_hdr = (struct arp_header *)(this->buf + sizeof *this->eth_hdr);

	/* Check if the ARP is a request or a replay */
	if ((this->arp_hdr->op == OP_REQUEST) || (this->arp_hdr->op == OP_REPLAY)) {
		if (this->arp_hdr->op == OP_REQUEST) {

			/* The ARP packet is a request, generate a replay and sent it back */
			generate_arp_replay(this);
			send_to_link(this->interface, this->buf, this->len);
		} else {

			/* The ARP packet is a replay, cache the source MAC address */
			cache_new_mac_addr(this->macs, this->arp_hdr->spa, this->arp_hdr->sha);

			/* Send every packet that was waiting for the received MAC address */
			while (!queue_empty(this->pckg_queue)) {
				packed_msg_t *pckg = queue_deq(this->pckg_queue);

				if (pckg->hop == this->arp_hdr->spa) {
					process_waiting_packet(this, pckg);
					send_to_link(this->interface, pckg->buf, this->len);
					free_packed_msg(pckg);
				} else {

					/*
					 * If the packet does not meet the requirements it is
					 * sent back in the waiting queue
					 */
					queue_enq(this->pckg_aux, (void *)pckg);
				}
			}

			/* Move the waiting packets from aux to the waiting queue */
			reinit_the_waiting_queue(this);
		}
	}
}

/**
 * @brief Initiates a router structure, computes the routing
 * table of the router following the binary trie principle,
 * allocates memory for the arp responses cache and allocated
 * memory for waiting queues and sets the the ipv4 and arp
 * handlers that are not visible for the user.
 * 
 * @param path 
 * @return router_t* 
 */
router_t* init_router(char *path) {
	router_t *new_router = malloc(sizeof *new_router);

	if (new_router != NULL) {
		new_router->routes = btrie_rtable(path);

		if (new_router->routes == NULL) {
			free(new_router);

			return NULL;
		}

		new_router->macs = create_vector();

		if (new_router->macs == NULL) {
			free_btrie(&new_router->routes);
			free(new_router);

			return NULL;
		}

		new_router->pckg_queue = queue_create();

		if (new_router->pckg_queue == NULL) {
			free_vector(&new_router->macs);
			free_btrie(&new_router->routes);
			free(new_router);

			return NULL;
		}

		new_router->pckg_aux = queue_create();

		if (new_router->pckg_aux == NULL) {
			free_vector(&new_router->macs);
			free_btrie(&new_router->routes);
			queue_free(new_router->pckg_aux);
			free(new_router);

			return NULL;
		}

		new_router->ipv4 = ipv4_handler;
		new_router->arp = arp_handler;

		new_router->next_hop = 0;
		new_router->interface = 0;
	}

	return new_router;
}

/**
 * @brief Frees the memory allocated for the router structure.
 * However this function is called just in router faults, because
 * the router runs in a infinite loop.
 * 
 * @param router the router structure that holds the router node.
 */
void free_router(router_t *router) {
	if (router != NULL) {
		if (router->pckg_aux != NULL) {
			queue_free(router->pckg_aux);
			router->pckg_aux = NULL;
		}

		if (router->pckg_queue != NULL) {
			queue_free(router->pckg_queue);
			router->pckg_queue = NULL;
		}

		if (router->macs != NULL) {
			free_vector(&router->macs);
		}

		if (router->routes != NULL) {
			free_btrie(&router->routes);
		}

		free(router);
	}
}

/**
 * @brief Checks if the packet received is of ipv4 type.
 * 
 * @param router the router structure that holds the router node.
 * @return uint8_t 1 if the packet is of ipv4 type or 0 otherwise.
 */
uint8_t packet_is_ipv4(router_t *router) {
	if ((router == NULL) || (router->eth_hdr->ether_type != IP_TYPE)) {
		return 0;
	}

	return 1;
}

/**
 * @brief Checks if the packet received is of arp type.
 * 
 * @param router the router structure that holds the router node.
 * @return uint8_t 1 if the packet is of arp type or 0 otherwise.
 */
uint8_t packet_is_arp(router_t *router) {
	if ((router == NULL) || (router->eth_hdr->ether_type != ARP_TYPE)) {
		return 0;
	}

	return 1;
}

/**
 * @brief Blockant action that waits to receive a packet
 * from the network.
 * 
 * @param router the router structure that holds the router node.
 * @return int the interface that received the packet.
 */
int recv_msg(router_t *router) {
	if (router != NULL) {
		router->len = 0;

		return recv_from_any_link(router->buf, &router->len);
	}

	return -1;
}

/**
 * @brief Initiate the ethernet header of the received packet
 * into the router structure.
 * 
 * @param router the router structure that holds the router node.
 */
void init_msg_fields(router_t *router) {
	if (router != NULL) {
		router->eth_hdr = (struct ether_header *)router->buf;
	}
}