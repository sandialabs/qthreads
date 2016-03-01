#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "argparsing.h"

#include <qthread/qthread.h>
#include <qthread/dictionary.h>
#include <qthread/hash.h>


#define EXPECTED_ENTRIES 4

int my_key_equals(void *first,
                  void *second);
int my_hashcode(void *string);

int my_key_equals(void *first,
                  void *second)
{
  //iprintf("Comparing %s %s\n", (char *)first, (char *)second);
  return !strcmp(first, second);
}

int my_hashcode(void *string)
{
  return qt_hash_bytes(string, strlen((char*)string), GOLDEN_RATIO);
}

void my_destructor(void *key, void *val)
{
  iprintf("\tdeleting value key=%p (%s), val=%p (%s)\n", key, key, val, val);
}

int main(int    argc,
         char **argv)
{
  void *ret_code;

  CHECK_VERBOSE();

  qthread_initialize();
  qt_dictionary *dict   = qt_dictionary_create(my_key_equals, my_hashcode, my_destructor);
  char          *mykey1 = "k1";
  char          *myval1 = "v1";

  char *mykey2 = "k2";
  char *myval2 = "v2";

  char *mykey3 = "k3";
  char *myval3 = "v3";

  ret_code = qt_dictionary_put(dict, mykey1, myval1);
  iprintf(" 1. Put exited with code %p\n", ret_code);
  assert(ret_code != NULL);

  ret_code = qt_dictionary_put(dict, mykey2, myval2);
  iprintf(" 3. Put exited with code %p(%s)\n", ret_code, (char *)ret_code);
  assert(ret_code != NULL);

  ret_code = qt_dictionary_put(dict, mykey3, myval3);
  iprintf(" 3. Put exited with code %p(%s)\n", ret_code, (char *)ret_code);
  assert(ret_code != NULL);

  qt_dictionary_iterator* iter = qt_dictionary_iterator_create(dict);
  list_entry* e = qt_dictionary_iterator_get(iter);
  assert(e);
}
