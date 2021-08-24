#ifndef __GZIP_H__
#define __GZIP_H__
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "zlib.h"


#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384
int gzip_compress_buf_init(z_stream *strm, int size, char * source);
int gzip_compress(z_stream *strm, char *dest, int flush);
int gzip_init(z_stream *strm);

#endif
