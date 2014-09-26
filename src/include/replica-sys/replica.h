/*
 * =====================================================================================
 *
 *       Filename:  replica.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/05/2014 12:53:17 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef REPLICA_H
#define REPLICA_H

struct node_t;

struct node_t* system_initialize(int node_id,const char* start_mode,const char* config_path,void(*user_cb)(int data_size,void* data,void* arg),void* arg);
void system_run(struct node_t* replica);
void system_exit(struct node_t* replica);

#endif
