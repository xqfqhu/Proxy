#ifndef __PROXY_H__
#define __PROXY_H__

#include <stdio.h>
#include "csapp.h"
#include <string.h>
#include "sbuf.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define NTHREAD 8

typedef struct {
    rio_t request_rio;
    char type[MAXLINE];
    char hostname[MAXLINE];
    char path[MAXLINE];
    char port[MAXLINE];
} request_t;

typedef struct cache_line_t cache_line_t;
typedef struct cache_line_t{
    size_t size;
    char hostname[MAXLINE];
    char path[MAXLINE];
    char *obj;
    cache_line_t *next;
    cache_line_t *prev;
} cache_line_t;

typedef struct{
    size_t size_left;
    sem_t mutex;
    sem_t w;
    int read_cnt;
    cache_line_t *head;
    cache_line_t *tail;
} cache_t;


/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
static int read_request(int fd_from, request_t * request);
static void forward_request(int fd_to, request_t * request);
static int forward_headers(int fd_to, request_t * request);
static void forward_response(int fd_from, int fd_to, request_t * request);
void clienterror(int fd, char *cause, char *errnum,
                char *shortmsg, char *longmsg);
static void * thread(void *vargp);
static void serve_client(int clientfd);
static int read_cache(int clientfd, request_t *request);
static void write_cache(request_t * request, char * obj, size_t obj_size);
static cache_line_t * cache_line_init(size_t size, request_t * request);
static void cache_line_deinit(cache_line_t * cache_line);
static void cache_init(cache_t * cache);

#endif