// need an include file
#include <stdlib.h>

void *qt_malloc(size_t size){
  return chpl_malloc(size);
}

void qt_free(void *ptr){
  chpl_free(ptr);
}

void *qt_calloc(size_t nmemb, size_t size) {
  return chpl_calloc(nmemb, size);
}

void *qt_realloc(void *ptr, size_t size) {
  return chpl_realloc(ptr, size);
}
