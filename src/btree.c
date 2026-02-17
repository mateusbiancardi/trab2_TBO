/*
 * Alunos: Mateus Biancardi da Silva - 2024203031
 *         Pedro Marchini Pereira - 2023100264
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "btree.h"
#include "btree_node.h"
#include "queue.h"

// #include "../include/btree.h"
// #include "../include/btree_node.h"
// #include "../include/queue.h"

typedef struct
{
    int ordem;
    long raiz;
    long prox_no;
} BTreeHeader;

struct BTree
{
    BTreeHeader header;
    FILE *fp;
};

static long node_offset(BTree *t, long idx)
{
    return sizeof(BTreeHeader) +
           idx * node_disk_size(t->header.ordem);
}

static void write_header(BTree *t)
{
    fseek(t->fp, 0, SEEK_SET);
    fwrite(&t->header, sizeof(BTreeHeader), 1, t->fp);
    fflush(t->fp);
}

static void read_header(BTree *t)
{
    fseek(t->fp, 0, SEEK_SET);
    fread(&t->header, sizeof(BTreeHeader), 1, t->fp);
}

static BTreeNode *read_node(BTree *t, long idx)
{
    BTreeNode *n = node_create(t->header.ordem, 0);
    node_read(t->fp, node_offset(t, idx), n);
    return n;
}

static void write_node(BTree *t, long idx, BTreeNode *n)
{
    node_write(t->fp, node_offset(t, idx), n);
}

BTree *btree_create(int ordem, const char *filename)
{
    BTree *t = malloc(sizeof(BTree));
    if (!t)
        return NULL;

    t->fp = fopen(filename, "w+b");
    if (!t->fp)
    {
        free(t);
        return NULL;
    }

    memset(&t->header, 0, sizeof(BTreeHeader));

    t->header.ordem = ordem;
    t->header.raiz = 0;
    t->header.prox_no = 1;

    write_header(t);

    BTreeNode *root = node_create(ordem, 1);
    write_node(t, 0, root);
    node_destroy(root);

    return t;
}

BTree *btree_open(const char *filename)
{
    BTree *t = malloc(sizeof(BTree));
    if (!t)
        return NULL;

    t->fp = fopen(filename, "r+b");
    if (!t->fp)
    {
        free(t);
        return NULL;
    }

    read_header(t);
    return t;
}

void btree_destroy(BTree *t)
{
    if (!t)
        return;
    write_header(t);
    fclose(t->fp);
    free(t);
}


static int search_rec(BTree *t, long idx, int key, int *out_reg)
{
    BTreeNode *n = read_node(t, idx);
    int i = 0;

    while (i < n->n_chaves && key > n->chaves[i])
        i++;

    if (i < n->n_chaves && key == n->chaves[i])
    {
        *out_reg = n->registros[i];
        node_destroy(n);
        return 1;
    }

    if (n->is_leaf)
    {
        node_destroy(n);
        return 0;
    }

    long next = n->filhos[i];
    node_destroy(n);
    return search_rec(t, next, key, out_reg);
}

int btree_search(BTree *t, int chave, int *out_reg)
{
    if (t->header.raiz == -1)
        return 0;
    return search_rec(t, t->header.raiz, chave, out_reg);
}


static void split_child(BTree *t, long parent_idx,
                        BTreeNode *parent, int i)
{
    int ordem = t->header.ordem;
    int mid = (t->header.ordem - 1) / 2;

    long y_idx = parent->filhos[i];
    BTreeNode *y = read_node(t, y_idx);

    long z_idx = t->header.prox_no++;
    BTreeNode *z = node_create(ordem, y->is_leaf);

    z->n_chaves =
        y->n_chaves - mid - 1;

    for (int j = 0;
         j < z->n_chaves;
         j++)
    {
        z->chaves[j] =
            y->chaves[j + mid + 1];

        z->registros[j] =
            y->registros[j + mid + 1];
    }

    if (!y->is_leaf)
    {
        for (int j = 0;
             j <= z->n_chaves;
             j++)
        {
            z->filhos[j] =
                y->filhos[j + mid + 1];
        }
    }

    y->n_chaves = mid;

    for (int j = parent->n_chaves; j >= i + 1; j--)
        parent->filhos[j + 1] = parent->filhos[j];

    parent->filhos[i + 1] = z_idx;

    for (int j = parent->n_chaves - 1; j >= i; j--)
    {
        parent->chaves[j + 1] = parent->chaves[j];
        parent->registros[j + 1] = parent->registros[j];
    }

    parent->chaves[i] = y->chaves[mid];
    parent->registros[i] = y->registros[mid];
    parent->n_chaves++;

    write_node(t, y_idx, y);
    write_node(t, z_idx, z);
    write_node(t, parent_idx, parent);
    write_header(t);

    node_destroy(y);
    node_destroy(z);
}

static void insert_rec(BTree *t, long idx,
                            BTreeNode *n,
                            int key, int reg) 
{
    int i = n->n_chaves - 1;

    if (n->is_leaf) {
        while (i >= 0 && key < n->chaves[i])
        {
            n->chaves[i + 1] = n->chaves[i];
            n->registros[i + 1] = n->registros[i];
            i--;
        }

        n->chaves[i + 1] = key;
        n->registros[i + 1] = reg;
        n->n_chaves++;

        write_node(t, idx, n);
    }
    else {
        while (i >= 0 && key < n->chaves[i])
            i--;
        i++;

        BTreeNode *child = read_node(t, n->filhos[i]);

        insert_rec(t, n->filhos[i], child, key, reg);

        node_destroy(child);
        child = read_node(t, n->filhos[i]);

        if (node_is_full(child, t->header.ordem))
        {
            split_child(t, idx, n, i);
        }

        node_destroy(child);
    }
}


void btree_insert(BTree *t, int chave, int registro)
{
    if (t->header.raiz == -1) {
        t->header.raiz = t->header.prox_no++;
        BTreeNode *nova_raiz = node_create(t->header.ordem, 1);
        nova_raiz->chaves[0] = chave;
        nova_raiz->registros[0] = registro;
        nova_raiz->n_chaves = 1;
        
        write_node(t, t->header.raiz, nova_raiz);
        write_header(t);
        node_destroy(nova_raiz);
        return;
    }

    int tmp;
    if (btree_search(t, chave, &tmp))
        return;

    BTreeNode *root = read_node(t, t->header.raiz);
    insert_rec(t, t->header.raiz, root, chave, registro);
    node_destroy(root);

    root = read_node(t, t->header.raiz);

    if (node_is_full(root, t->header.ordem))
    {
        long new_root_idx = t->header.prox_no++;
        BTreeNode *new_root =
            node_create(t->header.ordem, 0);

        new_root->filhos[0] = t->header.raiz;
        t->header.raiz = new_root_idx;

        split_child(t, new_root_idx, new_root, 0);

        write_node(t, new_root_idx, new_root);
        write_header(t);
        node_destroy(new_root);
    }

    node_destroy(root);
}

static void remove_from_leaf(BTreeNode *n, int idx)
{
    for (int i = idx; i < n->n_chaves - 1; i++)
    {
        n->chaves[i] = n->chaves[i + 1];
        n->registros[i] = n->registros[i + 1];
    }
    n->n_chaves--;
}

static void fill(BTree *t, long parent_idx, BTreeNode *parent, int idx) {
    if (idx != 0) {
        BTreeNode* sibling = read_node(t, parent->filhos[idx-1]);
        int min_keys = ((t->header.ordem + 1) / 2) - 1;
        
        if (sibling->n_chaves > min_keys) {
            node_destroy(sibling);
            borrow_from_prev(t, parent_idx, parent, idx);
            return;
        }
        
        node_destroy(sibling);
    }
    if (idx != parent->n_chaves) {
        BTreeNode* sibling = read_node(t, parent->filhos[idx+1]);
        int min_keys = ((t->header.ordem + 1) / 2) - 1;
        
        if (sibling->n_chaves > min_keys) {
            node_destroy(sibling);
            borrow_from_next(t, parent_idx, parent, idx);
            return;
        }

        node_destroy(sibling);
    }


    if (idx != parent->n_chaves) {
        merge(t, parent_idx, parent, idx);
        BTreeNode *child = read_node(t, parent->filhos[idx]);
        if (node_is_full(child, t->header.ordem))
        {
            split_child(t, parent_idx, parent, idx);
        }
        node_destroy(child);
    }
    else {
        merge(t, parent_idx, parent, idx-1);
        BTreeNode *child = read_node(t, parent->filhos[idx-1]);
        if (node_is_full(child, t->header.ordem))
        {
            split_child(t, parent_idx, parent, idx-1);
        }
        node_destroy(child);
    }
}

static void remove_rec(BTree *t, long idx, int key)
{
    BTreeNode *n = read_node(t, idx);
    int i = 0;

    while (i < n->n_chaves && key > n->chaves[i])
        i++; 

    if (i < n->n_chaves && key == n->chaves[i])
    {
        if (n->is_leaf)
        {
            remove_from_leaf(n, i);
            write_node(t, idx, n);
        }
        else
        {
            BTreeNode *esquerda = read_node(t, n->filhos[i]);
            BTreeNode *direita = read_node(t, n->filhos[i+1]);
            int min_keys = ((t->header.ordem + 1) / 2) - 1;
            if (esquerda->n_chaves > min_keys) {
                int pred = get_pred(t, n->filhos[i]);
                n->chaves[i] = pred;
                write_node(t, idx, n);
                remove_rec(t, n->filhos[i], pred);
            }
            else if (direita->n_chaves > min_keys) {
                int succ = get_succ(t, n->filhos[i+1]);
                n->chaves[i] = succ;
                write_node(t, idx, n);
                remove_rec(t, n->filhos[i+1], succ);
            }
            else {
                merge(t, idx, n, i);
                BTreeNode *child = read_node(t, n->filhos[i]);
                if (node_is_full(child, t->header.ordem)) {
                    split_child(t, idx, n, i);
                    node_destroy(n);
                    n = read_node(t, idx);
                }
                node_destroy(child); 
                remove_rec(t, n->filhos[i], key);
            }

            node_destroy(esquerda);
            node_destroy(direita);
        }
    }
    else
    {
        if (n->is_leaf)
        {
            node_destroy(n);
            return;
        }

        int min_keys = ((t->header.ordem + 1) / 2) - 1;

        BTreeNode *child = read_node(t, n->filhos[i]);
        if (child->n_chaves <= min_keys) {
            fill(t, idx, n, i);
            node_destroy(n);
            n = read_node(t, idx);
            i = 0;
            while (i < n->n_chaves && key > n->chaves[i])
                i++;
        }
        node_destroy(child);

        remove_rec(t, n->filhos[i], key);
    }

    node_destroy(n);
}

void btree_remove(BTree *t, int chave)
{
    if (t->header.raiz == -1)
        return;
        
    remove_rec(t, t->header.raiz, chave);
    BTreeNode *raiz = read_node(t, t->header.raiz);

    if (raiz->n_chaves == 0) {
        if (!raiz->is_leaf)
            t->header.raiz = raiz->filhos[0];
        else 
            t->header.raiz = -1;

        write_header(t);
    }

    node_destroy(raiz);
}

void btree_print(BTree *t, FILE *out)
{
    if (!t || t->header.raiz == -1)
        return;

    Queue *q = queue_create();
    queue_push(q, t->header.raiz);

    while (!queue_empty(q))
    {
        Queue *next = queue_create();

        while (!queue_empty(q))
        {
            long rrn = queue_pop(q);

            /* cria nó temporário */
            BTreeNode *node =
                node_create(t->header.ordem, 0);

            long offset =
                sizeof(BTreeHeader) +
                rrn * node_disk_size(t->header.ordem);

            node_read(t->fp, offset, node);

            fprintf(out, "[");

            for (int i = 0; i < node->n_chaves; i++)
            {
                fprintf(out, "key: %d, ",
                        node->chaves[i]);
            }

            fprintf(out, "]");

            if (!node->is_leaf)
            {
                for (int i = 0;
                     i <= node->n_chaves;
                     i++)
                {
                    if (node->filhos[i] != -1)
                        queue_push(next,
                                   node->filhos[i]);
                }
            }

            node_destroy(node);
        }

        fprintf(out, "\n");

        queue_destroy(q);
        q = next;
    }

    queue_destroy(q);
}

static int get_pred(BTree *t, long idx)
{
    BTreeNode *node = read_node(t, idx);

    while (!node->is_leaf) {
        idx = node->filhos[node->n_chaves];
        node_destroy(node);
        node = read_node(t, idx);
    }

    int key = node->chaves[node->n_chaves-1];

    node_destroy(node);

    return key;
}

static int get_succ(BTree *t, long idx)
{
    BTreeNode *node = read_node(t, idx);

    while (!node->is_leaf) {
        idx = node->filhos[0];
        node_destroy(node);
        node = read_node(t, idx);
    }

    int key = node->chaves[0];

    node_destroy(node);

    return key;
}

static void borrow_from_prev(BTree *t, long parent_idx, BTreeNode *parent, int idx) {
    BTreeNode* child = read_node(t, parent->filhos[idx]);
    BTreeNode* sibling = read_node(t, parent->filhos[idx-1]);

    for (int i = child->n_chaves; i >= 1; i--) {
        child->chaves[i] = child->chaves[i-1];
        child->registros[i] = child->registros[i-1];
    }
    
    if (!child->is_leaf) {
        for (int i = child->n_chaves + 1; i >= 1; i--) {
            child->filhos[i] = child->filhos[i-1];
        }
    }
    
    child->chaves[0] = parent->chaves[idx-1];
    child->registros[0] = parent->registros[idx-1];

    parent->chaves[idx-1] = sibling->chaves[sibling->n_chaves-1];
    parent->registros[idx-1] = sibling->registros[sibling->n_chaves-1];

    if (!sibling->is_leaf) {
        child->filhos[0] = sibling->filhos[sibling->n_chaves];
    }

    child->n_chaves++;
    sibling->n_chaves--;

    write_node(t, parent->filhos[idx], child);
    write_node(t, parent->filhos[idx-1], sibling);
    write_node(t, parent_idx, parent);

    node_destroy(child);
    node_destroy(sibling);
}

static void borrow_from_next(BTree *t, long parent_idx, BTreeNode *parent, int idx) {
    BTreeNode* child = read_node(t, parent->filhos[idx]);
    BTreeNode* sibling = read_node(t, parent->filhos[idx+1]);

    child->chaves[child->n_chaves] = parent->chaves[idx];
    child->registros[child->n_chaves] = parent->registros[idx];

    if (!child->is_leaf) {
        child->filhos[child->n_chaves + 1] = sibling->filhos[0];
    }

    parent->chaves[idx] = sibling->chaves[0];
    parent->registros[idx] = sibling->registros[0];

    for (int i = 0; i < sibling->n_chaves - 1; i++) {
        sibling->chaves[i] = sibling->chaves[i+1];
        sibling->registros[i] = sibling->registros[i+1];
    }

    if (!sibling->is_leaf) {
        for (int i = 0; i < sibling->n_chaves; i++) {  
            sibling->filhos[i] = sibling->filhos[i+1];
        }
    }

    child->n_chaves++;
    sibling->n_chaves--;

    write_node(t, parent->filhos[idx], child);
    write_node(t, parent->filhos[idx+1], sibling);
    write_node(t, parent_idx, parent);

    node_destroy(child);
    node_destroy(sibling);
}

static void merge(BTree *t, long parent_idx, BTreeNode *parent, int idx) {
    long child_idx = parent->filhos[idx];
    BTreeNode* child = read_node(t, parent->filhos[idx]);
    BTreeNode* sibling = read_node(t, parent->filhos[idx+1]);

    child->chaves[child->n_chaves] = parent->chaves[idx];
    child->registros[child->n_chaves] = parent->registros[idx];

    for (int i = 0; i < sibling->n_chaves; i++) {
        child->chaves[i + child->n_chaves + 1] = sibling->chaves[i];
        child->registros[i + child->n_chaves + 1] = sibling->registros[i];
    }

    if (!sibling->is_leaf) {
        for (int i = 0; i <= sibling->n_chaves; i++) {
            child->filhos[i + child->n_chaves + 1] = sibling->filhos[i];
        }
    }

    child->n_chaves += (1 + sibling->n_chaves);

    for (int i = idx + 1; i < parent->n_chaves; i++) {
        parent->chaves[i - 1] = parent->chaves[i];
        parent->registros[i - 1] = parent->registros[i];
    }
    
    for (int i = idx + 1; i <= parent->n_chaves; i++) {
        parent->filhos[i] = parent->filhos[i + 1];
    }
    
    parent->n_chaves--;

    write_node(t, child_idx, child);
    write_node(t, parent_idx, parent);

    node_destroy(child);
    node_destroy(sibling);
}