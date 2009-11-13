#ifndef TEST_ARGPARSING_H
#define TEST_ARGPARSING_H

#include <stdio.h>
#include <stdarg.h>

#define CHECK_INTERACTIVE() interactive = (getenv("TEST_INTERACTIVE") != NULL)
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

static int interactive = 0;

static void iprintf(char *format, ...)
{
    if (interactive != 0) {
	va_list ap;

	va_start(ap, format);
	vprintf(format, ap);
	fflush(stdout);
	va_end(ap);
    }
}

#endif
