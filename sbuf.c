#include "sbuf.h"


void sbuf_init(sbuf_t *sbuf, int n){
    sbuf->buf = Calloc(n, sizeof(int));
    sbuf->front = 0;
    sbuf->rear = 0;
    sbuf->n = n;
    Sem_init(&sbuf->items, 0, 0);
    Sem_init(&sbuf->slots, 0, n);
    Sem_init(&sbuf->mutex, 0, 1);
}

void sbuf_insert(sbuf_t *sbuf, int item){
    P(&(sbuf->slots));
    P(&(sbuf->mutex));
    (sbuf->buf)[(++sbuf->rear) % (sbuf->n)] = item;
    V(&(sbuf->mutex));
    V(&(sbuf->items));
}

int sbuf_remove(sbuf_t *sbuf){
    int item;
    P(&(sbuf->items)); //should go first. if you decrease mutex first, critical section might be locked when there is no item(deadlock)
    P(&(sbuf->mutex));
    item = (sbuf->buf)[(++sbuf->front) % (sbuf->n)];
    V(&(sbuf->mutex));
    V(&(sbuf->slots));
    
    return item;
}
void sbuf_deinit(sbuf_t *sbuf){
    free(sbuf->buf);
}

