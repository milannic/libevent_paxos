/*
 * =====================================================================================
 *
 *       Filename:  main.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/06/2014 04:31:43 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include "include/replica-sys/replica.h"

static void pseudo_cb(int data_size,void* data){
    printf("%d\n",(*(int*)data));
    free(data);
}

int main(int argc,char** argv){
    struct node_t* my_node = system_initialize(argc,argv,pseudo_cb);
    if(my_node){
        system_run(my_node);
    }
}