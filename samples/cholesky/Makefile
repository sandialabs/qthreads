include $(QT_CNC_INSTALL)/common.mk

QT_INCLUDES=-I. -I$(QT_INSTALL)/include -I$(QT_CNC_INSTALL)
QT_LIBS=-L$(QT_INSTALL)/lib -lqthread -lpthread -lrt
QT_ARGS=-std=c++0x -g -Wall -O3
CC=gcc
CPP=g++

TEST=cholesky
OBJS=$(TEST)_cnc.o $(TEST).o
USER_ARGS=-fwhole-program
BIN=bin
TARGET=$(BIN)/$(TEST)

$(TARGET): $(OBJS)
	$(CPP) $^  $(QT_LIBS) $(QT_ARGS) $(USER_ARGS) -o $@

%.o: %.cpp
	$(CPP) -c $< $(QT_INCLUDES) $(QT_ARGS)
	
%.o: %.cpp %.h
	$(CPP) -c $< $(QT_INCLUDES) $(QT_ARGS)

run$(TEST): $(TARGET)
	./$(TARGET) 1000 50 -i input/1000.in -o output/1000.out

test$(TEST): $(TARGET)
	./$(TARGET) 1000 50 -i input/1000.in -o output/1000.out
	@diff --ignore-space-change -q reference_output/1000.out output/1000.out
		
clean:
	-rm *.o $(TARGET) output/*

