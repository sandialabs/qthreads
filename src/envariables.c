#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h> /* for getenv() and strtoul() */
#include <stdio.h>  /* for fprintf() */

#include "qt_visibility.h"
#include "qt_envariables.h"
#include "qt_debug.h"

unsigned long INTERNAL qt_internal_get_env_num(const char   *envariable,
                                               unsigned long dflt,
                                               unsigned long zerodflt)
{
    char         *str = getenv(envariable);
    unsigned long tmp = dflt;

    qthread_debug(CORE_BEHAVIOR, "checking envariable %s\n", envariable);
    if (str && *str) {
        char *errptr;
        tmp = strtoul(str, &errptr, 0);
        if (*errptr != 0) {
            fprintf(stderr, "unparsable %s (%s)\n", envariable, str);
            tmp = dflt;
        }
        if (tmp == 0) {
            qthread_debug(CORE_DETAILS, "since envariable %s is 0, choosing default: %u\n", envariable, zerodflt);
            tmp = zerodflt;
        }
    }
    return tmp;
}

/* vim:set expandtab: */
