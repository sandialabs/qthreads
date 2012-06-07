include $(QT_CNC_INSTALL)/common.mk

QT_INCLUDES=-I. -I$(QT_INSTALL)/include -I$(QT_CNC_INSTALL) 
QT_LIBS=-L$(QT_INSTALL)/lib -lqthread -lpthread
QT_ARGS=-std=c++0x -g 
CC=gcc
CPP=g++

TEST=primes
OBJS=$(TEST).o
USER_ARGS=
BIN=bin
TARGET=$(BIN)/$(TEST)

$(TARGET): $(OBJS) 
	$(CPP) $^  $(QT_INCLUDES) $(QT_LIBS) $(QT_ARGS) $(USER_ARGS) -o $@

%.o: %.cpp
	$(CPP) $^ -c $(QT_INCLUDES) $(QT_ARGS) -o $@

run$(TEST): $(TARGET)
	./$(TARGET) 1000

test$(TEST): $(TARGET)
	./$(TARGET) 1000 167
		
clean:
	-rm *.o $(TARGET)

