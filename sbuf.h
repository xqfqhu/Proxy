#ifndef __SBUF_H__
#define __SBUF_H__

#include <semaphore.h>
#include <pthread.h>
#include <stddef.h>
#include "csapp.h"

typedef struct{
    int * buf;
    int n;
    int front;
    int rear;
    sem_t items;
    sem_t slots;
    sem_t mutex;
} sbuf_t;

void sbuf_init(sbuf_t *sbuf, int n);
void sbuf_insert(sbuf_t *sbuf, int item);
int sbuf_remove(sbuf_t *sbuf);
void sbuf_deinit(sbuf_t *sbuf);

#endif