/*
 * =====================================================================================
 *
 *       Filename:  message.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/29/2014 03:44:08 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef MESSAGE_H
#define MESSAGE_H

#include "common-header.h"


typedef enum paxos_msg_code_t{
    // for future usage, we reserve some numbers.
    // heart beat
        ping_req=0,
        ping_ack=1,
    // normal case
        accept_req=10,
        accept_ack=11,

    // leader election
        l_prepare_req=20,
        l_prepare_ack=21,
        l_accept_req=22,
        l_accept_ack=23,
        l_commit_req=24, //used when there is a final leader
        l_commit_ack=25, // used when
    // client communication with 
        c_request_submit_req = 40,
        c_request_submit_ack = 41,
}paxos_msg_code;

typedef struct paxos_msg_t{
    size_t data_size;
    paxos_msg_code msg_code;
    char data[0];
}paxos_msg;
#define PAXOS_MSG_SIZE(M) (M->data_size+sizeof(paxos_msg))

typedef struct ping_msg_t{
    int node_id;
    view node_view;
    struct timeval timestamp;
}ping_msg;
#define PING_REQ_MSG(M) (sizeof(ping_req_msg))


#endif
