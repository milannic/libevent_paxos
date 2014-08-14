#!/bin/python
import os
import re
import subprocess
import argparse


def run_test(path):
    cur_dir=path

    total_passed_tests=0
    total_tests=0

    if os.path.exists(cur_dir+"/log"):
        for i in os.listdir(cur_dir+"/log"):
            os.remove(cur_dir+"/log/"+i)
    else:
        os.mkdir(cur_dir+"/log")

    for i in os.listdir(path):
        if None!=re.match(r'.*test.*(?<!swp)$',i) and None==re.match(r'.*\.py$',i):
            total_tests = total_tests+1
            print "==========Test No %d=========="%(total_tests)
            print "==========Test Start=========="
            ret=subprocess.call(path+"/"+i,shell=True)
            print "\n"
            if ret:
                print "Test Failed"
            else:
                print "Test Passed"
                total_passed_tests += 1;
            print "\n"
            print "==========Test End=========="
            print "\n"

    print "There are %d / %d Tests Passed\n"%(total_passed_tests,total_tests)
def main():
    parser = argparse.ArgumentParser("Run All The Tests Scripts In The Folder");
    parser.add_argument("-p","--path",dest="path",type=str,help="Path For Test \
            Script Folder",required=True)
    para_sets=parser.parse_args();
    run_test(para_sets.path)

if __name__=="__main__":
    main()
