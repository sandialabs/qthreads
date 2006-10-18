#include "qalloc.h"
#include "qthread.h"

#include <stdio.h>		       /* for perror() */
#include <stdlib.h>		       /* for exit() */
#include <sys/types.h>		       /* for mmap() */
#include <sys/mman.h>		       /* for mmap() */
#include <sys/stat.h>		       /* for open() */
#include <fcntl.h>		       /* for open() */
#include <unistd.h>		       /* for fstat() */
#include <inttypes.h>		       /* for funky print statements */

static struct mapinfo_s
{
    void *map;
    size_t size;
    void ***streams;
    size_t streamcount;
    struct mapinfo_s *next;
}        *mmaps = NULL;

#ifdef __linux__
# define fstat fstat64
# define lseek lseek64
typedef struct stat64 statstruct_t;

#elif defined(__APPLE__)
# define O_NOATIME 0
typedef struct stat statstruct_t;

#endif

void *qalloc_makemap(const off_t filesize, void *addr, const char *filename,
		     size_t itemsize, const size_t streams)
{				       /*{{{ */
    void *set, *ret;
    int fd, rcount, fstatret;
    statstruct_t st;

    fd = open(filename, O_RDWR | O_CREAT | O_NOATIME | O_NONBLOCK,
	      S_IRUSR | S_IWUSR);
    if (fd == -1) {
	perror("open");
	abort();
    }
    fstatret = fstat(fd, &st);
    if (fstatret != 0) {
	perror("fstat");
	abort();
    }
    if (st.st_size == 0) {
	/* new file */
	off_t tailend;
	char touch = 0;

	tailend = lseek(fd, filesize - 1, SEEK_SET);
	if (tailend != filesize - 1) {
	    perror("seeking to end of file");
	    abort();
	}
	if (write(fd, &touch, 1) != 1) {
	    perror("setting file size");
	    abort();
	}
	tailend = lseek(fd, 0, SEEK_SET);
	if (tailend != 0) {
	    perror("seeking back to beginning of file");
	    abort();
	}
    } else if (st.st_size != filesize) {
	fprintf(stderr,
		"file is the wrong size! Wanted %" PRIuMAX " but got %"
		PRIuMAX "\n", (uintmax_t) filesize, (uintmax_t) st.st_size);
	abort();
    }
    rcount = read(fd, &set, sizeof(void *));
    if (rcount != sizeof(void *)) {
	perror("reading base ptr");
	abort();
    }
    ret =
	mmap(addr, (size_t) filesize, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
	     0);
    printf("mmap called with filesize %" PRIuMAX " returned %p\n",
	   (uintmax_t) filesize, ret);
    if (ret == NULL || ret == (void *)-1) {
	/* could not mmap() */
	perror("mmap");
	abort();
    } else if (set == NULL) {
	void **ptr = (void **)ret;
	char *base = (char *)(ptr + 3 + streams);
	size_t i;
	void ***strms;
	struct mapinfo_s *mi;

	/* never mmapped anything before */
	ptr[0] = ret;
	/* 32-bit alignment */
	itemsize = (((itemsize - 1) / 4) + 1) * 4;
	ptr[1] = (void *)itemsize;
	ptr[2] = (void *)streams;
	strms = malloc(sizeof(void **) * streams);
	mi = malloc(sizeof(struct mapinfo_s));
	mi->map = ret;
	mi->size = (size_t) filesize;
	mi->streams = (void ***)(ptr + 3);
	mi->streamcount = streams;
	mi->next = mmaps;
	mmaps = mi;
	/* initialize the streams */
	i = 0;
	for (i = 0; i < streams; ++i) {
	    strms[i] = ptr[3 + i] = (void *)(base + (itemsize * i));
	}
	base += itemsize * streams;
	/* now fill in the free lists */
	while ((void *)(base + (itemsize * streams)) < ret + filesize) {
	    for (i = 0; i < streams; ++i) {
		strms[i] = *strms[i] = base + (itemsize * i);
	    }
	    base += itemsize * streams;
	}
	for (i = 0; i < streams; ++i) {
	    if ((void *)(base + (itemsize * i)) >= ret + filesize)
		break;
	    strms[i] = *strms[i] = base + (itemsize * i);
	}
	free(strms);
	/* and just for safety's sake, let's sync it to disk */
	if (msync(ret, (size_t) filesize, MS_INVALIDATE | MS_SYNC) != 0) {
	    perror("msync");
	    //abort();
	}
	return mi;
    } else if (set != ret) {
	/* asked for it somewhere that it didn't appear */
	printf("offset: %i\n", (int)(set - ret));
	abort();
    } else {
	/* reloading an existing file in the correct place */
	struct mapinfo_s *m;
	m = malloc(sizeof(struct mapinfo_s));
	m->map = ret;
	m->size = (size_t) filesize;
	m->streams = (void ***)(((void **)ret) + 3);
	m->streamcount = streams;
	m->next = mmaps;
	mmaps = m;
	return m;
    }
}				       /*}}} */

void *qalloc_loadmap(const char *filename)
{
    int fd, fstatret;
    statstruct_t st;
    off_t filesize;
    void **header[3];

    fd = open(filename, O_RDONLY, S_IRUSR | S_IWUSR);
    if (fd == -1) {
	perror("open");
	abort();
    }
    fstatret = fstat(fd, &st);
    if (fstatret != 0) {
	perror("fstat");
	abort();
    }
    filesize = st.st_size;
    if (read(fd, header, sizeof(header)) != sizeof(header)) {
	perror("read");
	abort();
    }
    if (close(fd) != 0) {
	perror("close");
	abort();
    }
    return qalloc_makemap(filesize, header[0], filename, (size_t) (header[1]),
			  (size_t) (header[2]));
}

void qalloc_cleanup()
{
    qalloc_checkpoint();
    while (mmaps) {
	struct mapinfo_s *m;

	if (munmap(mmaps->map, mmaps->size) != 0) {
	    perror("munmap");
	    abort();
	}
	m = mmaps;
	mmaps = mmaps->next;
	free(m);
    }
}

void *qalloc_malloc(struct mapinfo_s *m)
{
    pthread_t me = pthread_self();
    size_t stream = (size_t) me % m->streamcount;
    void **ret = m->streams[stream];

    printf("malloc from stream %llu\n", (long long unsigned)stream);
    m->streams[stream] = *(m->streams[stream]);
    *ret = 0x0000000000000000LL;       /* remove the pointer from it (unnecessary) */
    return ret;
}

void qalloc_free(void *block, struct mapinfo_s *m)
{
    pthread_t me = pthread_self();
    size_t stream = (size_t) me % m->streamcount;
    void **b = (void **)block;

    printf("freeing into stream %llu\n", (long long unsigned)stream);
    *b = m->streams[stream];
    m->streams[stream] = b;
}

void qalloc_checkpoint()
{
    struct mapinfo_s *m = mmaps;

    while (m) {
	if (msync(m->map, m->size, MS_INVALIDATE | MS_SYNC) != 0) {
	    perror("checkpoint");
	    //abort();
	}
	m = m->next;
    }
}
