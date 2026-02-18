/*
 * Alunos: Mateus Biancardi da Silva - 2024203031
 *         Pedro Marchini Pereira - 2023100264
 */

#include <stdlib.h>
#include "queue.h"
// #include "../include/queue.h"

// fila auxiliar usada para imprimir a árvore b por nível
typedef struct QNode {
    long value;  // índice do nó no arquivo
    struct QNode *next;
} QNode;

struct Queue {
    QNode *front;
    QNode *rear;
};

Queue *queue_create(void)
{
    Queue *q = malloc(sizeof(Queue));
    q->front = q->rear = NULL;
    return q;
}

void queue_destroy(Queue *q)
{
    // remove todos os elementos antes de liberar a fila
    while (!queue_empty(q))
        queue_pop(q);
    free(q);
}

int queue_empty(Queue *q)
{
    return q->front == NULL;
}

void queue_push(Queue *q, long value)
{
    QNode *n = malloc(sizeof(QNode));
    n->value = value;
    n->next = NULL;

    if (q->rear)
        q->rear->next = n;
    else
        q->front = n;  // fila estava vazia

    q->rear = n;
}

long queue_pop(Queue *q)
{
    QNode *n = q->front;
    long v = n->value;

    q->front = n->next;
    if (!q->front)
        q->rear = NULL;  // fila ficou vazia

    free(n);
    return v;
}
