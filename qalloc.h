#ifndef QALLOC_H
#define QALLOC_H

#define _FILE_OFFSET_BITS 64

#include <sys/types.h>
#include <unistd.h>

struct mapinfo_s;
struct dynmapinfo_s;

void *qalloc_makestatmap(const off_t filesize, void *addr,
			 const char *filename, size_t itemsize,
			 const size_t streams);
void *qalloc_makedynmap(const off_t filesize, void *addr,
			const char *filename, const size_t streams);
void *qalloc_loadmap(const char *filename);
void qalloc_checkpoint();
void qalloc_cleanup();

void *qalloc_statmalloc(struct mapinfo_s *);
void *qalloc_dynmalloc(struct dynmapinfo_s *m, size_t size);
void *qalloc_malloc(void *, size_t size);

void qalloc_statfree(void *block, struct mapinfo_s *m);
void qalloc_dynfree(void *block, struct dynmapinfo_s *m);
void qalloc_free(void *block, void *m);

#endif
