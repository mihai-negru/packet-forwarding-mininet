#ifndef VECTOR_H_
#define VECTOR_H_

#include <stdio.h>
#include <string.h>

#include "lib.h"

#define MAX_VECTOR_SIZE 100
#define MAC_ADDR_SIZE 6

typedef struct vector_s {
    struct arp_entry *addrs;
    int len;
} vector_t;

vector_t*   create_vector       (void);
void        free_vector         (vector_t **vec);
void        cache_new_mac_addr  (vector_t *vec, uint32_t new_ip, uint8_t new_mac[6]);
int         get_mac_entry       (vector_t *macs, uint32_t given_ip);

#endif /* VECTOR_H_ */