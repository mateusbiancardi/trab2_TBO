/*
 * Alunos: Mateus Biancardi da Silva - 2024203031
 *         Pedro Marchini Pereira - 2023100264
 */

#ifndef QUEUE_H
#define QUEUE_H

typedef struct Queue Queue;

Queue *queue_create(void);
void   queue_destroy(Queue *q);
int    queue_empty(Queue *q);
void   queue_push(Queue *q, long value);
long   queue_pop(Queue *q);

#endif
