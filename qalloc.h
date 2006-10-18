#ifndef QALLOC_H
#define QALLOC_H

#include <sys/types.h>
#include <unistd.h>

struct mapinfo_s;

void *qalloc_makemap(const off_t filesize, void *addr, const char *filename,
		     size_t itemsize, const size_t streams);
void *qalloc_loadmap(const char *filename);
void qalloc_cleanup();
void *qalloc_malloc(struct mapinfo_s *);
void qalloc_free(void *block, struct mapinfo_s *m);
void qalloc_checkpoint();

#endif
