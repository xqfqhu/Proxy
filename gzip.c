#include "gzip.h"

int gzip_init(z_stream *strm){
    
    int ret;
    
    strm->zalloc = Z_NULL;
    strm->zfree = Z_NULL;
    strm->opaque = Z_NULL;
    
    ret = deflateInit2(strm, Z_DEFAULT_COMPRESSION,Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY);
    
    if (ret != Z_OK)
    
        return ret;
}

int gzip_compress_buf_init(z_stream *strm, int size, char * source){
    
    strm->avail_in = size;
    strm->next_in = source;
}

int gzip_compress(z_stream *strm, char *dest, int flush){
    int ret;
    int compressed_size;
    strm->avail_out = CHUNK;
    strm->next_out = dest;
    ret = deflate(strm, flush);
    compressed_size = CHUNK - strm->avail_out;
    assert(ret != Z_STREAM_ERROR);

    return compressed_size;
}