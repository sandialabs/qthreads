#ifndef QT_MACROS_H
#define QT_MACROS_H

#include <threads.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __GNUC__
#define Q_UNUSED(x) __attribute__((unused)) x
#else
#define Q_UNUSED(x) x
#endif

#define TLS_DECL(type, name) thread_local type name
#define TLS_DECL_INIT(type, name) thread_local type name = 0
#define TLS_GET(name) name
#define TLS_SET(name, val) name = (val)
#define TLS_INIT(name)
#define TLS_INIT2(name, func)
#define TLS_DELETE(name) name = 0

#endif // ifndef QT_MACROS_H
/* vim:set expandtab: */
