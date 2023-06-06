#include "binary_trie.h"

/**
 * @brief Create a btrie node object
 * 
 * @return btrie_node_t* returns an empty binary trie node.
 */
static btrie_node_t* create_btrie_node(void) {
    btrie_node_t *new_node = malloc(sizeof *new_node);

    if (new_node != NULL) {
        new_node->type = EMPTY;
        new_node->hop = 0;
        new_node->interface = -1;

        new_node->left = NULL;
        new_node->right = NULL;
    }

    return new_node;
}

/**
 * @brief Create a btrie object.
 * 
 * @return btrie_t* returns an empty binary trie.
 */
btrie_t* create_btrie(void) {
    btrie_t *new_tree = malloc(sizeof *new_tree);

    if (new_tree != NULL) {
        new_tree->root = create_btrie_node();

        if (new_tree->root == NULL) {
            free(new_tree);
            new_tree = NULL;
        } else {
            new_tree->root->type = DUMMY;
            new_tree->size = 0;
        }
    }

    return new_tree;
}

static void free_btrie_nodes(btrie_node_t *__restrict__ bnode) {
    if (bnode != NULL) {
        free_btrie_nodes(bnode->left);
        free_btrie_nodes(bnode->right);

        free(bnode);
    }
}

/**
 * @brief Frees the memory allocated for the tree.
 * 
 * @param tree a pointer to the binary trie.
 */
void free_btrie(btrie_t **__restrict__ tree) {
    if ((tree != NULL) && (*tree != NULL)) {
        free_btrie_nodes((*tree)->root);

        free(*tree);
        *tree = NULL;
    }
}

static void fill_btrie_node(btrie_node_t *__restrict__ bnode, uint32_t hop, int interface) {
    if (bnode != NULL) {
        bnode->type = INFO;
        bnode->hop = hop;
        bnode->interface = interface;
    }
}

/**
 * @brief Inserts an entry into the binary trie
 * 
 * @param tree the binary trie
 * @param prefix the prefix to insert in the trie
 * @param mask the mask of the prefix
 * @param hop the hop described by the prefix
 * @param interface the interface described by the prefix
 */
void btrie_insert(btrie_t *__restrict__ tree, uint32_t prefix, uint32_t mask, uint32_t hop, int interface) {
    if (tree != NULL) {
        uint32_t prefix_length = 0;
        uint32_t iter_mask = 0;
        iter_mask = mask;

        /* Compute how much bits to insert in the trie */
        while (iter_mask != 0) {
            prefix_length += (iter_mask & 1);
            iter_mask >>= 1;
        }

        /* Insert the number of bits that are covered by the mask in network order */
        if (prefix_length != 0) {
            
            btrie_node_t *iter_node = tree->root;

            uint32_t iter_prefix = 0;
            iter_prefix = (prefix & mask);

            while ((prefix_length--) != 0) {
                uint32_t next_bit = (iter_prefix & 1);
                iter_prefix >>= 1;

                if (next_bit == 0) {
                    if (iter_node->left == NULL) {
                        iter_node->left = create_btrie_node();
                    }

                    iter_node = iter_node->left;
                } else if (next_bit == 1) {
                    if (iter_node->right == NULL) {
                        iter_node->right = create_btrie_node();
                    }

                    iter_node = iter_node->right;
                } else {
                    return;
                }
            }

            fill_btrie_node(iter_node, hop, interface);
            ++(tree->size);
        }
    }
}

/**
 * @brief Computes the longest prefix match.
 * 
 * @param tree the binary trie containing all the prefixes
 * @param addr the address to match
 * @return hop_info_t* an allocated structure to extract the information about the prefix
 */
hop_info_t* btrie_lpm(btrie_t *__restrict tree, uint32_t addr) {
    if ((tree == NULL) || (tree->root == NULL)) {
        return NULL;
    }

    hop_info_t *lpm_route = malloc(sizeof *lpm_route);
    lpm_route->status = INVALID;

    if (lpm_route != NULL) {
        btrie_node_t *iter_node = tree->root;

        while (iter_node != NULL) {
            if (iter_node->type == INFO) {
                lpm_route->hop = iter_node->hop;
                lpm_route->interface = iter_node->interface;
                lpm_route->status = VALID;
            }

            uint32_t next_bit = (addr & 1);
            addr >>= 1;

            if (next_bit == 0) {
                iter_node = iter_node->left;
            } else if (next_bit == 1) {
                iter_node = iter_node->right;
            } else {
                free(lpm_route);
                lpm_route = NULL;

                break;
            }
        }

        if (lpm_route->status == INVALID) {
            free(lpm_route);
            lpm_route = NULL;
        }
    }

    return lpm_route;
}

/**
 * @brief Reads a file of routes and parses them to the binary trie
 * 
 * @param filename the file to read the routes
 * @return btrie_t* an allocated completed binary trie
 */
btrie_t* btrie_rtable(const char *filename) {
    if (filename == NULL) {
        return NULL;
    }

    btrie_t *ip_trie = create_btrie();

    if (ip_trie != NULL) {
        FILE *fin = fopen(filename, "r");

        if (fin == NULL) {
            free_btrie(&ip_trie);
        } else {
            char *byte;
            char line[MAX_LINE_SIZE];

            while (fgets(line, MAX_LINE_SIZE, fin) != NULL) {
                uint32_t prefix = 0;
                uint32_t next_hop = 0;
                uint32_t mask = 0;
                int interface = 0;

                byte = strtok(line, " .");

                int byte_idx = 0;
                while (byte != NULL) {
                    if (byte_idx < 4) {
                        *(((unsigned char *)&prefix)  + byte_idx % 4) = (unsigned char)atoi(byte);
                    } else if (byte_idx < 8) {
                        *(((unsigned char *)&next_hop)  + byte_idx % 4) = (unsigned char)atoi(byte);
                    } else if (byte_idx < 12) {
                        *(((unsigned char *)&mask)  + byte_idx % 4) = (unsigned char)atoi(byte);
                    } else if (byte_idx == 12) {
                        interface = atoi(byte);
                    }

                    byte = strtok(NULL, " .");
                    ++byte_idx;
                }

                btrie_insert(ip_trie, prefix, mask, next_hop, interface);
            }

            fclose(fin);
        }
    }

    return ip_trie;
}