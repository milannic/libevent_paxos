CUR_DIR=$(shell pwd)
LPATH=$(CUR_DIR)/.local/lib
LDFLAGS= -L$(LPATH) -levent -lconfig
OTHEROPT= -D DEBUG=1
IPATH=-I. -I$(CUR_DIR)/.local/include
CFLAGS=-Wall -g -std=gnu11 $(OTHEROPT) $(IPATH)
GCC=gcc

PROGRAM=server.out

SOURCE=$(shell ls *c)

OBJ=$(SOURCE:.c=.o)


.PHONY:default
default:$(PROGRAM)

%.o: %.c
	$(GCC) $(CFLAGS) -c -o $@ $^


server.out:$(OBJ)
	$(GCC) $(CFLAGS) -o $@  $^ $(LDFLAGS) 

.PHONY:clean
clean:
	rm -rf $(PROGRAM)
	rm -rf $(OBJ)

.PHONY:display
display:
	@echo $(OBJ)
	@echo $(LPATH)

.PHONY:test
test:
	$(shell cd Test;bash ./ping_test;cd ..)

