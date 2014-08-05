/*
 * =====================================================================================
 *
 *       Filename:  primary.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/05/2014 02:45:11 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef LEADER_H 
#define LEADER_H
#include "common-header.h"
#include "message.h"

struct node_t;
typedef void (*req_send_callback)(struct node_t* my_node,view_stamp req_id,size_t data_size,void* data);

typedef struct leader_t{
    req_send_callback local_cb;
    view_stamp next_stamp;
    view_stamp previous_commited_stamp;
    // hashtable
    // request stack
    // hash_table 
}leader;

void send_req(leader* la,size_t data_size,void* data);
void check_unfinished_req(leader* la);
void resend_req(leader* la,view_stamp vs);
void handle_req_ack(leader* la,view_stamp vs);

#endif
