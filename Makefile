LDFLAGS=-levent -lconfig
CFLAGS=-Wall -g -std=c99
IPATH=.
GCC=gcc


PROGRAM=server

SOURCE=$(shell ls *c)

OBJ=$(SOURCE:.c=.o)


.PHONY:default
default:$(PROGRAM)


%.o:%.c %h
	$(GCC) -I $(IPATH)  $(CFLAGS) -o $@  $^


server:$(OBJ)
	$(GCC) -I $(IPATH)  $(CFLAGS) -o $@  $^ $(LDFLAGS)

.PHONY:clean
clean:
	rm -rf $(PROGRAM)
	rm -rf $(OBJ)

.PHONY:display
display:
	@echo $(OBJ)
