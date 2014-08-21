.PHONY: default clean test count


CUR_DIR := $(shell pwd)
FILELIST:=$(shell find ./src -type f)

default:
	cd target;$(MAKE);cd ..;

clean:
	cd target;$(MAKE) clean;cd ..;

test:
	@python ./test/run_test.py -p ${CUR_DIR}/test

count:
	@wc -l ${FILELIST} 

