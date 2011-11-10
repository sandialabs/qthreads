#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h> /* for getenv() and strtoul() */
#include <stdio.h>  /* for fprintf() */

#include "qt_visibility.h"
#include "qt_envariables.h"
#include "qt_debug.h"

const char INTERNAL *qt_internal_get_env_str(const char *envariable)
{
    char        mod_envariable[100];
    const char *str;

    assert(strlen(envariable) < 90);

    snprintf(mod_envariable, 100, "QT_%s", envariable);
    str = getenv(mod_envariable);
    qthread_debug(CORE_BEHAVIOR, "checking envariable %s\n", envariable);
    if (str && *str) {
        return str;
    } else {
        snprintf(mod_envariable, 100, "QTHREAD_%s", envariable);
        str = getenv(mod_envariable);
        if (str && *str) {
            return str;
        }
    }
    return NULL;
}

unsigned long INTERNAL qt_internal_get_env_num(const char   *envariable,
                                               unsigned long dflt,
                                               unsigned long zerodflt)
{
    const char   *str = qt_internal_get_env_str(envariable);
    unsigned long tmp = dflt;

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
        } else {
            qthread_debug(CORE_DETAILS, "envariable %s parsed as %u\n", envariable, tmp);
        }
    }
    return tmp;
}

/* vim:set expandtab: */
