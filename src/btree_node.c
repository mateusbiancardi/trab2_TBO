/*
 * Alunos: Mateus Biancardi da Silva - 2024203031
 *         Pedro Marchini Pereira - 2023100264
 */

#include <stdlib.h>
#include <stdio.h>
#include "btree_node.h"
// #include "../include/btree_node.h"

// gerencia criação, destruição e i/o em disco dos nós da árvore b
BTreeNode *node_create(int ordem, int is_leaf)
{
    BTreeNode *n = malloc(sizeof(BTreeNode));
    if (!n) return NULL;

    n->ordem = ordem;
    n->is_leaf = is_leaf;
    n->n_chaves = 0;

    // aloca arrays dinamicamente baseado na ordem
    n->chaves    = calloc(ordem, sizeof(int));
    n->registros = calloc(ordem, sizeof(int));
    n->filhos    = calloc(ordem+1, sizeof(long));  // ordem+1 ponteiros

    if (!n->chaves || !n->registros || !n->filhos) {
        free(n);
        return NULL;
    }

    // inicializa filhos como inválidos
    for (int i = 0; i < ordem; i++)
        n->filhos[i] = -1;

    return n;
}

void node_destroy(BTreeNode *n)
{
    if (!n) return;
    free(n->chaves);
    free(n->registros);
    free(n->filhos);
    free(n);
}

int node_is_full(BTreeNode *n, int ordem)
{
    return n->n_chaves == ordem;
}

// escreve nó em posição específica do arquivo
void node_write(FILE *fp, long offset, BTreeNode *n)
{
    fseek(fp, offset, SEEK_SET);

    // ordem de escrita: metadados, chaves, registros, filhos
    fwrite(&n->is_leaf, sizeof(int), 1, fp);
    fwrite(&n->n_chaves, sizeof(int), 1, fp);

    fwrite(n->chaves, sizeof(int), n->ordem, fp);
    fwrite(n->registros, sizeof(int), n->ordem, fp);
    fwrite(n->filhos, sizeof(long), n->ordem + 1, fp);

    fflush(fp);
}

// lê nó de posição específica do arquivo
void node_read(FILE *fp, long offset, BTreeNode *n)
{
    fseek(fp, offset, SEEK_SET);

    // ordem de leitura deve ser idêntica à de escrita
    fread(&n->is_leaf, sizeof(int), 1, fp);
    fread(&n->n_chaves, sizeof(int), 1, fp);

    fread(n->chaves, sizeof(int), n->ordem, fp);
    fread(n->registros, sizeof(int), n->ordem, fp);
    fread(n->filhos, sizeof(long), n->ordem + 1, fp);
}

// calcula espaço ocupado por um nó no disco
long node_disk_size(int ordem)
{
    return sizeof(int) * 2 +              // is_leaf + n_chaves
           sizeof(int) * (ordem) +        // chaves
           sizeof(int) * (ordem) +        // registros
           sizeof(long) * (ordem+1);      // filhos
}
