include $(QT_CNC_INSTALL)/common.mk

QT_INCLUDES=-I. -I$(QT_INSTALL)/include -I$(QT_CNC_INSTALL)
QT_LIBS=-L$(QT_INSTALL)/lib -lqthread -lpthread -lrt
QT_ARGS=-std=c++0x -g -Wall -fauto-inc-dec -fcompare-elim -fcprop-registers -fdce -fdefer-pop -fdse -fguess-branch-probability -fif-conversion2  -fif-conversion -fipa-pure-const \
          -fipa-profile -fipa-reference -fmerge-constants -fsplit-wide-types -ftree-bit-ccp -ftree-builtin-call-dce -ftree-ccp -ftree-ch \
          -ftree-copyrename -ftree-dce -ftree-dominator-opts -ftree-dse -ftree-forwprop -ftree-fre -ftree-phiprop -ftree-sra -ftree-pta -ftree-ter  -funit-at-a-time \
			\
			-fthread-jumps -falign-functions  -falign-jumps -falign-loops  -falign-labels -fcaller-saves -fcrossjumping -fcse-follow-jumps  -fcse-skip-blocks \
			-fdelete-null-pointer-checks -fdevirtualize -fexpensive-optimizations -fgcse  -fgcse-lm -finline-small-functions -findirect-inlining -fipa-sra \
			-foptimize-sibling-calls -fpartial-inlining -fpeephole2 -fregmove -freorder-blocks  -freorder-functions -frerun-cse-after-loop -fsched-interblock  -fsched-spec \
			-fschedule-insns  -fschedule-insns2 -fstrict-aliasing -fstrict-overflow -ftree-switch-conversion -ftree-tail-merge -ftree-pre  -ftree-vrp \
			\
			-finline-functions -funswitch-loops -fpredictive-commoning -fgcse-after-reload -ftree-vectorize -fipa-cp-clone 
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
	diff --ignore-space-change -q reference_output/1000.out output/1000.out
		
clean:
	-rm *.o $(TARGET) output/*

