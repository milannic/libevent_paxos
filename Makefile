LDFLAGS=-levent -lconfig
CFLAGS=-Wall -g -std=c99 $(OTHEROPT)
OTHEROPT= -D DEBUG=1
IPATH=.
LPATH=$(HOME)/.local/lib
GCC=gcc

PROGRAM=server.out

SOURCE=$(shell ls *c)

OBJ=$(SOURCE:.c=.o)


.PHONY:default
default:$(PROGRAM)

%.o: %.c
	$(GCC) -L$(LPATH) -I$(IPATH) $(CFLAGS) -c -o $@ $^


server.out:$(OBJ)
	$(GCC) -L$(LPATH) -I$(IPATH)  $(CFLAGS) -o $@  $^ $(LDFLAGS) 

.PHONY:clean
clean:
	rm -rf $(PROGRAM)
	rm -rf $(OBJ)

.PHONY:display
display:
	@echo $(OBJ)
	@echo $(LPATH)
