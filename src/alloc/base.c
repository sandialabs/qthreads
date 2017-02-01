// need an include file
#include <stdlib.h>

void *qt_malloc(size_t size){
  return malloc(size);
}

void qt_free(void *ptr){
  free(ptr);
}

void *qt_calloc(size_t nmemb, size_t size) {
  return calloc(nmemb, size);
}

void *qt_realloc(void *ptr, size_t size) {
  return realloc(ptr, size);
}
