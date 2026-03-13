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

// implementação de árvore b com persistência em disco

// metadados persistidos no início do arquivo
typedef struct
{
    int ordem;
    long raiz;      // índice do nó raiz
    long prox_no;   // próximo índice livre
} BTreeHeader;

struct BTree
{
    BTreeHeader header;
    FILE *fp;
};

// calcula posição de um nó no arquivo baseado no seu índice
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

// cria nova árvore b em disco
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
    t->header.raiz = 0;          // raiz é sempre índice 0
    t->header.prox_no = 1;       // próximo nó será índice 1

    write_header(t);

    // cria raiz vazia como folha
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


// busca recursiva na árvore
static int search_rec(BTree *t, long idx, int key, int *out_reg)
{
    BTreeNode *n = read_node(t, idx);
    int i = 0;

    // encontra posição da chave no nó
    while (i < n->n_chaves && key > n->chaves[i])
        i++;

    // chave encontrada
    if (i < n->n_chaves && key == n->chaves[i])
    {
        *out_reg = n->registros[i];
        node_destroy(n);
        return 1;
    }

    // chegou na folha sem encontrar
    if (n->is_leaf)
    {
        node_destroy(n);
        return 0;
    }

    // desce para filho apropriado
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


// divide nó cheio em dois
static void split_child(BTree *t, long parent_idx,
                        BTreeNode *parent, int i)
{
    int ordem = t->header.ordem;
    int mid = (t->header.ordem - 1) / 2;  // índice do meio

    long y_idx = parent->filhos[i];
    BTreeNode *y = read_node(t, y_idx);  // nó cheio a ser dividido

    long z_idx = t->header.prox_no++;     // aloca novo nó
    BTreeNode *z = node_create(ordem, y->is_leaf);

    // copia metade superior para novo nó z
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

    // copia ponteiros se não for folha
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

    y->n_chaves = mid;  // ajusta tamanho do nó original

    // abre espaço no pai para novo filho
    for (int j = parent->n_chaves; j >= i + 1; j--)
        parent->filhos[j + 1] = parent->filhos[j];

    parent->filhos[i + 1] = z_idx;

    // sobe chave do meio para o pai
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

// inserção recursiva na árvore
static void insert_rec(BTree *t, long idx,
                            BTreeNode *n,
                            int key, int reg) 
{
    int i = n->n_chaves - 1;

    if (n->is_leaf) {
        // abre espaço e insere na folha
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
        // encontra filho apropriado
        while (i >= 0 && key < n->chaves[i])
            i--;
        i++;

        BTreeNode *child = read_node(t, n->filhos[i]);

        insert_rec(t, n->filhos[i], child, key, reg);

        // re-lê filho pois pode ter sido modificado
        node_destroy(child);
        child = read_node(t, n->filhos[i]);

        // divide se ficou cheio
        if (node_is_full(child, t->header.ordem))
        {
            split_child(t, idx, n, i);
        }

        node_destroy(child);
    }
}


// insere chave na árvore
void btree_insert(BTree *t, int chave, int registro)
{
    // árvore estava vazia (após remoções)
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

    // não insere duplicatas
    int tmp;
    if (btree_search(t, chave, &tmp))
        return;

    BTreeNode *root = read_node(t, t->header.raiz);
    insert_rec(t, t->header.raiz, root, chave, registro);
    node_destroy(root);

    // verifica se raiz precisa ser dividida
    root = read_node(t, t->header.raiz);

    if (node_is_full(root, t->header.ordem))
    {
        // cria nova raiz com a raiz antiga como filho
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

// garante que filho tem chaves suficientes antes de descer
static void fill(BTree *t, long parent_idx, BTreeNode *parent, int idx) {
    // tenta pegar emprestado do irmão anterior
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
    // tenta pegar emprestado do irmão seguinte
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


    // não conseguiu emprestar, faz merge com irmão
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

// remoção recursiva
static void remove_rec(BTree *t, long idx, int key)
{
    BTreeNode *n = read_node(t, idx);
    int i = 0;

    // encontra posição da chave
    while (i < n->n_chaves && key > n->chaves[i])
        i++; 

    // caso 1: chave está neste nó
    if (i < n->n_chaves && key == n->chaves[i])
    {
        // caso 1a: chave em folha - só remove
        if (n->is_leaf)
        {
            remove_from_leaf(n, i);
            write_node(t, idx, n);
        }
        // caso 1b: chave em nó interno
        else
        {
            BTreeNode *esquerda = read_node(t, n->filhos[i]);
            BTreeNode *direita = read_node(t, n->filhos[i+1]);
            int min_keys = ((t->header.ordem + 1) / 2) - 1;
            // substitui por predecessor se filho esquerdo tem chaves extras
            if (esquerda->n_chaves > min_keys) {
                int pred = get_pred(t, n->filhos[i]);
                n->chaves[i] = pred;
                write_node(t, idx, n);
                remove_rec(t, n->filhos[i], pred);
            }
            // substitui por sucessor se filho direito tem chaves extras
            else if (direita->n_chaves > min_keys) {
                int succ = get_succ(t, n->filhos[i+1]);
                n->chaves[i] = succ;
                write_node(t, idx, n);
                remove_rec(t, n->filhos[i+1], succ);
            }
            // ambos filhos têm mínimo - faz merge e remove recursivamente
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
    // caso 2: chave não está neste nó
    else
    {
        // chegou em folha sem encontrar
        if (n->is_leaf)
        {
            node_destroy(n);
            return;
        }

        int min_keys = ((t->header.ordem + 1) / 2) - 1;

        // garante que filho tem chaves suficientes antes de descer
        BTreeNode *child = read_node(t, n->filhos[i]);
        if (child->n_chaves <= min_keys) {
            fill(t, idx, n, i);
            // re-lê pai pois fill pode ter alterado
            node_destroy(n);
            n = read_node(t, idx);
            // reposiciona índice após fill
            i = 0;
            while (i < n->n_chaves && key > n->chaves[i])
                i++;
        }
        node_destroy(child);

        remove_rec(t, n->filhos[i], key);
    }

    node_destroy(n);
}

// remove chave da árvore
void btree_remove(BTree *t, int chave)
{
    if (t->header.raiz == -1)
        return;
        
    remove_rec(t, t->header.raiz, chave);
    BTreeNode *raiz = read_node(t, t->header.raiz);

    // raiz ficou vazia - promove único filho ou marca árvore vazia
    if (raiz->n_chaves == 0) {
        if (!raiz->is_leaf)
            t->header.raiz = raiz->filhos[0];  // promove filho
        else 
            t->header.raiz = -1;  // árvore ficou vazia

        write_header(t);
    }

    node_destroy(raiz);
}

// imprime árvore por níveis (level-order)
void btree_print(BTree *t, FILE *out)
{
    if (!t || t->header.raiz == -1)
        return;

    Queue *q = queue_create();
    queue_push(q, t->header.raiz);

    while (!queue_empty(q))
    {
        Queue *next = queue_create();  // fila para próximo nível

        // processa todos os nós do nível atual
        while (!queue_empty(q))
        {
            long rrn = queue_pop(q);

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

            // adiciona filhos na fila do próximo nível
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

// busca predecessor (maior chave da subárvore esquerda)
static int get_pred(BTree *t, long idx)
{
    BTreeNode *node = read_node(t, idx);

    // desce sempre pelo filho mais à direita
    while (!node->is_leaf) {
        idx = node->filhos[node->n_chaves];
        node_destroy(node);
        node = read_node(t, idx);
    }

    int key = node->chaves[node->n_chaves-1];  // última chave da folha

    node_destroy(node);

    return key;
}

// busca sucessor (menor chave da subárvore direita)
static int get_succ(BTree *t, long idx)
{
    BTreeNode *node = read_node(t, idx);

    // desce sempre pelo filho mais à esquerda
    while (!node->is_leaf) {
        idx = node->filhos[0];
        node_destroy(node);
        node = read_node(t, idx);
    }

    int key = node->chaves[0];  // primeira chave da folha

    node_destroy(node);

    return key;
}

// pega chave emprestada do irmão anterior (rotação à direita)
static void borrow_from_prev(BTree *t, long parent_idx, BTreeNode *parent, int idx) {
    BTreeNode* child = read_node(t, parent->filhos[idx]);
    BTreeNode* sibling = read_node(t, parent->filhos[idx-1]);

    // abre espaço no início do filho
    for (int i = child->n_chaves; i >= 1; i--) {
        child->chaves[i] = child->chaves[i-1];
        child->registros[i] = child->registros[i-1];
    }
    
    if (!child->is_leaf) {
        for (int i = child->n_chaves + 1; i >= 1; i--) {
            child->filhos[i] = child->filhos[i-1];
        }
    }
    
    // move chave do pai para filho
    child->chaves[0] = parent->chaves[idx-1];
    child->registros[0] = parent->registros[idx-1];

    // move última chave do irmão para pai
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

// pega chave emprestada do irmão seguinte (rotação à esquerda)
static void borrow_from_next(BTree *t, long parent_idx, BTreeNode *parent, int idx) {
    BTreeNode* child = read_node(t, parent->filhos[idx]);
    BTreeNode* sibling = read_node(t, parent->filhos[idx+1]);

    // move chave do pai para final do filho
    child->chaves[child->n_chaves] = parent->chaves[idx];
    child->registros[child->n_chaves] = parent->registros[idx];

    if (!child->is_leaf) {
        child->filhos[child->n_chaves + 1] = sibling->filhos[0];
    }

    // move primeira chave do irmão para pai
    parent->chaves[idx] = sibling->chaves[0];
    parent->registros[idx] = sibling->registros[0];

    // desloca elementos do irmão para esquerda
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

// combina filho com irmão seguinte
static void merge(BTree *t, long parent_idx, BTreeNode *parent, int idx) {
    long child_idx = parent->filhos[idx];
    BTreeNode* child = read_node(t, parent->filhos[idx]);
    BTreeNode* sibling = read_node(t, parent->filhos[idx+1]);

    // desce chave do pai para o filho
    child->chaves[child->n_chaves] = parent->chaves[idx];
    child->registros[child->n_chaves] = parent->registros[idx];

    // copia todas as chaves do irmão para o filho
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

    // remove chave e ponteiro do pai
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