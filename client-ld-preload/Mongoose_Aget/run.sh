#! /bin/bash

CLIENT_PROGRAM=./aget 
LD_PRELOAD=./../libclilib.so ${CLIENT_PROGRAM} -f -n2 -p 9000 http://localhost/input.tar.gz
