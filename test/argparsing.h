#ifndef TEST_ARGPARSING_H
#define TEST_ARGPARSING_H

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#ifndef SST
# define CHECK_VERBOSE() verbose = (getenv("VERBOSE") != NULL)
#else
# define CHECK_VERBOSE()
#endif

#define NUMARG(var,name) do { \
    char *str; \
    if ((str = getenv(name)) != NULL) { \
	char *stre = NULL; \
	size_t tmp = strtoul(str, &stre, 0); \
	if (stre == NULL || stre == str) { \
	    fprintf(stderr, "unparsable "name" (%s)\n", str); \
	} else { \
	    var = tmp; \
	} \
    } \
    iprintf(name" = %lu\n", (unsigned long)var); \
} while (0)

#define NUMARRARG(var,name,size) do { \
    char *split_str = getenv(name); \
    if (split_str != NULL) { \
        char *rest = NULL; \
        for (int i = 0; i < size; i++) { \
            var[i] = strtoul(split_str, &rest, 0); \
            assert(rest != NULL && rest != split_str); \
            split_str = rest + 1; \
        } \
        if (*rest != '\0') { \
            fprintf(stderr, "too many "name" arguments.\n"); \
        } \
    } \
    iprintf(name" = [%lu", (unsigned long)var[0]); \
    for (int i = 1; i < size; i++) \
        iprintf(", %lu", (unsigned long)var[i]); \
    iprintf("]\n"); \
} while (0)

#ifdef SST
static int verbose = 1;
#else
static int verbose = 0;
#endif

#if defined(__tile__) || defined(__CYGWIN32__)
# define iprintf printf
#else
static void iprintf(const char * restrict format, ...)
{
    if (verbose != 0) {
	va_list ap;

	va_start(ap, format);
	vprintf(format, ap);
	fflush(stdout);
	va_end(ap);
    }
}
#endif

#endif
