#! /usr/bin/env python2.7
import argparse
import xlwt


def main():
    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("-z","--zero",action="store_true")
    arg_parser.add_argument("-p","--proxy",dest="proxy_log_path",type=str,required=True);
    arg_parser.add_arguent("-c","--consensus",dest="consensus_log_path",type=str,required=True);
    options = arg_parser.parse_args()
    print options.proxy_log_path
    print("hello")
    pass

if __name__=="__main__":
    main()

