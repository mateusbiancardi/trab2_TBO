/*
 * Alunos: Mateus Biancardi da Silva - 2024203031
 *         Pedro Marchini Pereira - 2023100264
 */

#include <stdlib.h>
#include "queue.h"
// #include "../include/queue.h"

typedef struct QNode {
    long value;
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
        q->front = n;

    q->rear = n;
}

long queue_pop(Queue *q)
{
    QNode *n = q->front;
    long v = n->value;

    q->front = n->next;
    if (!q->front)
        q->rear = NULL;

    free(n);
    return v;
}
