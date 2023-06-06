#include "utils.h"

int main(int argc, char *argv[]) {
	init(argc - 2, argv + 2);

	router_t *router = init_router(argv[1]);

	while (1) {
		router->interface = recv_msg(router);
		
		if (router->interface < 0) {
			free_router(router);

			DEBUG("Interfaces are corrupted!!!");

			exit(-1);
		}

		init_msg_fields(router);

		if (packet_is_ipv4(router) || packet_is_arp(router)) {
			if (packet_is_ipv4(router)) {
				router->ipv4(router);
			} else {
				router->arp(router);
			}
		} else {
			DEBUG("Type unidentified...dropping.");
		}
	}
}

