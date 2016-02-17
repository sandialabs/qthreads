#ifndef LOGGING_H
#define LOGGING_H
#include<stdio.h>

#define INNERSTRING(x) #x
#define STRING(x) INNERSTRING(x)

#ifndef LOGERR
# define LOGERR 1
#endif
#ifndef LOGWARN
# define LOGWARN 1
#endif
// Rely on compiler to optimize out calls that will not be executed,
// because group should always evaluate to a constant at compile
// time. To use this, make preprocessor definitions of the different
// error groups you want to have available and set them to either 0 or
// 1 according to what you want to see.
#define logargs(group, s, args...) do {                                 \
    if(group) {                                                         \
      printf("(" #group ") " __FILE__ ":" STRING(__LINE__) ": " s "\n", args); \
    }                                                                   \
  } while(0)

#define log(group, s) do {                                            \
    if(group) {                                                       \
      printf("(" #group ") " __FILE__ ":" STRING(__LINE__) ": " s "\n");   \
    }                                                                 \
  } while(0)



#endif
