#ifndef BINARY_TRIE_H_
#define BINARY_TRIE_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAX_LINE_SIZE 64

typedef enum hop_status_s {
    VALID,
    INVALID
} hop_status_t;

typedef struct hop_info_s {
    uint32_t hop;
    int interface;
    hop_status_t status;
} hop_info_t;

typedef enum bnode_status_s {
    DUMMY,
    EMPTY,
    INFO
} bnode_status_t;

typedef struct btrie_node_s {
    bnode_status_t type;
    uint32_t hop;
    int interface;
    struct btrie_node_s *left;
    struct btrie_node_s *right;
} btrie_node_t;

typedef struct btrie_s {
    btrie_node_t *root;
    size_t size;
} btrie_t;

btrie_t*        create_btrie    (void);
void            free_btrie      (btrie_t **__restrict__ tree);
void            btrie_insert    (btrie_t *__restrict__ tree, uint32_t prefix, uint32_t mask, uint32_t hop, int interface);
hop_info_t*     btrie_lpm       (btrie_t *__restrict tree, uint32_t addr);
btrie_t*        btrie_rtable    (const char *filename);

#endif /* BINARY_TRIE_H_ */
