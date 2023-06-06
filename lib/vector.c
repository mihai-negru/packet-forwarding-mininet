#include "vector.h"

/**
 * @brief Create a vector object
 * 
 * @return vector_t* returns an enpty vector structure
 */
vector_t* create_vector() {
    vector_t *new_vec = malloc(sizeof *new_vec);

    if (new_vec != NULL) {
        new_vec->addrs = malloc(sizeof *new_vec->addrs * MAX_VECTOR_SIZE);

        if (new_vec->addrs == NULL) {
            free(new_vec);

            new_vec = NULL;
        } else {
            new_vec->len = 0;
        }
    }

    return new_vec;
}

/**
 * @brief Frees the memory allocated by a vector
 * 
 * @param vec pointer to vector
 */
void free_vector(vector_t **vec) {
    if ((vec != NULL) && (*vec != NULL)) {
        free((*vec)->addrs);
        free(*vec);

        *vec = NULL;
    }
}

/**
 * @brief Registers a new MAC address in the cache
 * 
 * @param vec structure to register the address
 * @param new_ip the ip address of the interface
 * @param new_mac the mac address of the interface
 */
void cache_new_mac_addr(vector_t *macs, uint32_t new_ip, uint8_t new_mac[6]) {
    if ((macs != NULL) && (macs->addrs != NULL)) {
        macs->addrs[macs->len].ip = new_ip;
        memcpy(macs->addrs[macs->len].mac, new_mac, MAC_ADDR_SIZE);

        ++(macs->len);
    }
}

/**
 * @brief Get the mac address of an ip interface stored in cache.
 * 
 * @param macs the cache memory.
 * @param given_ip the ip interface
 * @return int a positive number or -1 if the mac address
 * was not found in the cache.
 */
int get_mac_entry(vector_t *macs, uint32_t given_ip) {
    if (macs != NULL) {
        for (int i = 0; i < macs->len; ++i) {
		    if (macs->addrs[i].ip == given_ip) {
			    return i;
		    }
	    }   
    }

	return -1;
}