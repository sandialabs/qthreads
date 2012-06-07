CC=gcc
CPP=g++

SHELL=/bin/bash

testall: testsamples

testsamples: 
	$(MAKE) test -C samples

.PHONY: cleansamples	
cleansamples:
	$(MAKE) clean -C samples

.PHONY: clean	
clean: cleansamples 
	-rm *.o

