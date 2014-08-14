.PHONY: default clean test 

CUR_DIR := $(shell pwd)

default:
	cd target;$(MAKE);cd ..;

clean:
	cd target;$(MAKE) clean;cd ..;

test:
	@python ./test/run_test.py -p ${CUR_DIR}/test

