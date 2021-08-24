#include "proxy.h"

sbuf_t sbuf;
cache_t cache;

int main(int argc, char **argv)
{
    int listenfd, clientfd, serverfd;
    char client_hostname[MAXLINE], client_port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    
    pthread_t pid;
    
    if (argc != 2) {
	    fprintf(stderr, "usage: %s <port>\n", argv[0]);
	    exit(1);
    }
    cache_init(&cache);
    listenfd = Open_listenfd(argv[1]);

    sbuf_init(&sbuf, NTHREAD);
    for (int i = 0; i < NTHREAD; i++){
        Pthread_create(&pid, NULL, thread, NULL);
    }

    while (1){
        /* accept request from client */
        clientlen = sizeof(clientaddr);
        clientfd = Accept(listenfd, (struct sockaddr *) &clientaddr, &clientlen);
        Getnameinfo((struct sockaddr *) &clientaddr, clientlen, client_hostname, MAXLINE, 
                    client_port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", client_hostname, client_port);
        sbuf_insert(&sbuf, clientfd);
        
    }

    printf("%s\n", user_agent_hdr);



    return 0;
}
static void * thread(void *vargp){

    Pthread_detach(Pthread_self());

    while(1){
        int clientfd = sbuf_remove(&sbuf);
        serve_client(clientfd);
        Close(clientfd);
    }
}

static void serve_client(int clientfd){
    int serverfd;
    request_t request;
    /* read request */
    if (read_request(clientfd, &request) < 0) return;

    /* try to read from cache */
    if (read_cache(clientfd, &request)) {
        printf("Read response from cache\n");
        return;
    }
    /* build connection with server */
    serverfd = Open_clientfd(request.hostname, request.port);
    printf("Built connection to (%s, %s)\n", request.hostname, request.port);


    /* forward request to server */
    forward_request(serverfd, &request);
    if (forward_request_headers(serverfd, &request) < 0) return;
    printf("Forwarded request to (%s, %s)\n", request.hostname, request.port);
    forward_response(serverfd, clientfd, &request);
    printf("Forwarded response\n");
    /* close connection */
    Close(serverfd);
}

static int read_request(int fd_from, request_t * request){

    char buf[MAXLINE], uri[MAXLINE], version[MAXLINE];

    char * request_path_tmp = request->path;
    memcpy(request_path_tmp, "/", 1);
    request_path_tmp++;


    Rio_readinitb(&(request->request_rio), fd_from);

    /* read the request */
    if (Rio_readlineb(&(request->request_rio), buf, MAXLINE) <= 0) return -1;

    sscanf(buf, "%s http://%s %s", request->type, uri, version); //parse request_type
    if (strcasecmp(request->type, "GET")) {                     

        clienterror(fd_from, request->type, "501", "Not Implemented", "Proxy does not implement this method");
        return -1;
    }  
    //error here
    if (strchr(uri, ':') != NULL){
        sscanf(uri, "%[^:]:%[^/]/%s", request->hostname, request->port, request_path_tmp); 
        // parse request_hostname, request_port, request_path if port is provided
    }
    else{
        sscanf(uri, "%[^/]/%s", request->hostname, request_path_tmp);
        memcpy(request->port, "80", 2); 
        // parse request_hostname, request_port, request_path if port is not provided
    }
    return 0;
}




static void forward_request(int fd_to, request_t * request){
    char buf[MAXLINE];
    sprintf(buf, "%s %s %s\r\n", request->type, request->path, "HTTP/1.0");
    printf("Processing request: %s %s %s from host %s\n",request->type, request->path, "HTTP/1.0", request->hostname);
    Rio_writen(fd_to, buf, strlen(buf));
    return;
}

static int forward_request_headers(int fd_to, request_t * request){
    char buf[MAXLINE];
    char header_key[MAXLINE], header_val[MAXLINE];
    int client_host_header_flag = 0;

    request->enable_compression = 0;

    if (Rio_readlineb(&(request->request_rio), buf, MAXLINE) <= 0) return -1;

    while (strcmp(buf, "\r\n")){
        
        sscanf(buf, "%[^:]: %s", header_key, header_val);
        if(!strcasecmp(header_key, "HOST")){
            client_host_header_flag = 1;
            Rio_writen(fd_to, buf, strlen(buf));
        }
        else if (!strcasecmp(header_key, "User-Agent"));
        else if (!strcasecmp(header_key, "Connection"));
        else if (!strcasecmp(header_key, "Proxy-Connection"));
        else{
            if (!strcasecmp(header_key, "Accept-Encoding")){
                if (strstr(header_val, "gzip")){
                    request->enable_compression = 1;
                }
            }
            Rio_writen(fd_to, buf, strlen(buf));
        }
        if (Rio_readlineb(&(request->request_rio), buf, MAXLINE) <= 0) return -1;
    }

    if (!client_host_header_flag){
        sprintf(buf, "HOST: %s\r\n", request->hostname);
        Rio_writen(fd_to, buf, strlen(buf));
    }
    sprintf(buf, "%s", user_agent_hdr);
    Rio_writen(fd_to, buf, strlen(buf));
    Rio_writen(fd_to, "Connection: close\r\n", strlen("Connection: close\r\n"));
    Rio_writen(fd_to, "Proxy-Connection: close\r\n", strlen("Proxy-Connection: close\r\n"));
    Rio_writen(fd_to, "\r\n", strlen("\r\n"));
    return 0;

}
static void forward_response(int fd_from, int fd_to, request_t * request){
    response_t response;
    response_header_t headers;
    response.raw_body_length = 0;
    response.compressed_body_length = 0;
    response.headers.num_headers = 0;
    response.forwarded_length = 0;

    char buf_from[CHUNK];
    char cache_local[MAX_OBJECT_SIZE];
    char header_key[MAXLINE], header_val[MAXLINE];
    size_t raw_read = 0;
    int flush = Z_NO_FLUSH;
    int cur_read;
    int cur_compressed;

    if (request->enable_compression == 1){
        response.request_compression = 1;
    }
    else{
        response.request_compression = 0;
        }

    Rio_readinitb(&(response.response_rio), fd_from);
    while (Rio_readlineb(&(response.response_rio), buf_from, CHUNK) > 0){
        if (strcmp(buf_from, "\r\n") == 0 || response.headers.num_headers == MAX_HEADERS - 1){
            break;
        }

        sscanf(buf_from, "%[^:]: %s\r\n", header_key, header_val);     
        
        if (!strcasecmp(header_key, "Content-Encoding")){
            if (strstr(header_val, "gzip")){
                
                response.request_compression = 0;
            }
        }
        if (!strcasecmp(header_key, "Content-length")){
            response.raw_body_length = atoi(header_val);
            response.headers.length_header_index = response.headers.num_headers;
        }
        cur_read = strlen(buf_from);
        
        memcpy(response.headers.headers[response.headers.num_headers], buf_from, cur_read);

        response.headers.num_headers++;
        
    }
    
    if (response.request_compression == 1){
        
        if (gzip_init(&(response.strm)) != Z_OK){
            response.request_compression = 0;
            forward_and_cache_response_headers(&response, cache_local, fd_to);
        }
    }
    else{
        forward_and_cache_response_headers(&response, cache_local, fd_to);
    }



    
    while ((cur_read = Rio_readnb(&(response.response_rio), buf_from, CHUNK)) > 0){

        raw_read += cur_read;
        if (raw_read == response.raw_body_length){
            flush = Z_FINISH;
        }
        if (response.request_compression == 1){
            gzip_compress_buf_init(&(response.strm), cur_read, buf_from);
            do{
                cur_compressed = gzip_compress(&(response.strm), response.compressed_body + response.compressed_body_length, flush);
                response.compressed_body_length += cur_compressed;
            }while (response.strm.avail_out == 0);
            assert (response.strm.avail_in == 0);
        }
        else{
            forward_and_cache_bytes(buf_from, cur_read, fd_to, &(response.forwarded_length), cache_local);
        }
        
        
    }
    if (response.request_compression == 1){
        
        sprintf(response.headers.headers[response.headers.length_header_index], "Content-length: %ld\r\n",response.compressed_body_length);
        forward_and_cache_response_headers(&response, cache_local, fd_to);
        forward_and_cache_bytes(response.compressed_body, response.compressed_body_length, fd_to, &(response.forwarded_length), cache_local);
        deflateEnd(&(response.strm));
    }

    if (response.forwarded_length <= MAX_OBJECT_SIZE) {
        
        write_cache(request, cache_local, response.forwarded_length);
        
    }
    printf("wrote to cache\n");
    return;
}
static void forward_and_cache_bytes(char * buffer, size_t size, int fd_to, size_t * size_forwarded, char * cache_local){
    Rio_writen(fd_to, buffer, size);
    if ((* size_forwarded) <= MAX_OBJECT_SIZE){
        memcpy(cache_local + (*size_forwarded), buffer, size);
        (* size_forwarded) += size;
    }
}

static void forward_and_cache_response_headers(response_t * response, char * cache_local, int fd_to){
    int header_length;
    for (int i = 0; i < response->headers.num_headers; i++){
        header_length = strlen(response->headers.headers[i]);
        forward_and_cache_bytes(response->headers.headers[i], header_length, fd_to, &(response->forwarded_length), cache_local);
    }
    if (response->request_compression == 1){
        header_length = strlen("Content-Encoding: gzip\r\n");
        forward_and_cache_bytes("Content-Encoding: gzip\r\n", header_length, fd_to, &(response->forwarded_length), cache_local);
    }
    header_length = strlen("\r\n");
    forward_and_cache_bytes("\r\n", header_length, fd_to, &(response->forwarded_length), cache_local);
}
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Tiny Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Tiny Web server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}


static int read_cache(int clientfd, request_t *request){
    P(&(cache.mutex));
    (cache.read_cnt)++;
    if (cache.read_cnt == 1) P(&(cache.w));
    V(&(cache.mutex));

    cache_line_t * cur = cache.head;
    while (cur != NULL){
        if (!strcasecmp(cur->hostname, request->hostname) && 
            !strcasecmp(cur->path, request->path)){
                Rio_writen(clientfd, cur->obj, cur->size);
                if (cur->prev != NULL) cur->prev->next = cur->next;
                if (cur->next != NULL) cur->next->prev = cur->prev;
                if (cur->next == NULL && cur->prev != NULL) cache.tail = cur->prev;
                cur->prev = NULL;
                cur->next = cache.head;
                cache.head = cur;
                break;
            }
        else cur = cur->next;
    }

    P(&(cache.mutex));
    cache.read_cnt--;
    if (cache.read_cnt == 0) V(&(cache.w));
    V(&(cache.mutex));

    if (cur != NULL) return 1;
    return 0;

}
static void write_cache(request_t * request, char * obj, size_t obj_size){
    P(&(cache.w));
    cache_line_t * new_cache_line = cache_line_init(obj_size, request);

    cache_line_t * tail = cache.tail;

    size_t tail_size;
    if (tail != NULL) tail_size = tail->size;

    memcpy(new_cache_line->obj, obj, obj_size);

    while (cache.size_left < obj_size){
        if (cache.head == cache.tail) cache.head = NULL;
        cache.tail = tail->prev;
        cache_line_deinit(tail);
        cache.size_left += tail_size;
        tail = cache.tail;
        tail_size = tail->size;

    }

    new_cache_line->next = cache.head;
    if (cache.head != NULL) (cache.head)->prev = new_cache_line;
    cache.head = new_cache_line;
    if (cache.tail == NULL) cache.tail = new_cache_line;
    cache.size_left -= obj_size;

    V(&(cache.w));
}



static cache_line_t * cache_line_init(size_t size, request_t * request){
    cache_line_t * cache_line;
    char * obj;
    cache_line = Malloc(sizeof(cache_line));
    obj = Malloc(size);
    cache_line->obj = obj;
    cache_line->size = size;
    memcpy(cache_line->hostname, request->hostname, MAXLINE);
    memcpy(cache_line->path, request->path, MAXLINE);
    cache_line->next = NULL;
    cache_line->prev = NULL;
    return cache_line;
}
static void cache_line_deinit(cache_line_t * cache_line){
    Free(cache_line->obj);
    Free(cache_line);
}

static void cache_init(cache_t * cache){
    cache->head = NULL;
    cache->tail = NULL;
    cache->size_left = MAX_CACHE_SIZE;
    Sem_init(&(cache->mutex), 0, 1);
    Sem_init(&(cache->w), 0 , 1);
    cache->read_cnt = 0;
}