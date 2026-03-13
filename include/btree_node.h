/*
 * Alunos: Mateus Biancardi da Silva - 2024203031
 *         Pedro Marchini Pereira - 2023100264
 */

#ifndef BTREE_NODE_H
#define BTREE_NODE_H

#include <stdio.h>

typedef struct {
    int ordem;
    int is_leaf;
    int n_chaves;

    int *chaves;
    int *registros;
    long *filhos;
} BTreeNode;

BTreeNode *node_create(int ordem, int is_leaf);
void node_destroy(BTreeNode *n);

int  node_is_full(BTreeNode *n, int ordem);

void node_write(FILE *fp, long offset, BTreeNode *n);
void node_read(FILE *fp, long offset, BTreeNode *n);

long node_disk_size(int ordem);

#endif
