/*
 * =====================================================================================
 *
 *       Filename:  consensus_msg.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/06/2014 03:13:37 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef CONSENSUS_MSG_H
#define CONSENSUS_MSG_H

#include "../util/common-header.h"

typedef struct accept_req_t{
}accept_req;

typedef struct accept_ack_t{
}accept_ack;

typedef struct missing_req_t{
}missing_req;

typedef struct missing_ack_t{
}missing_ack;

typedef enum paxos_msg_code_t{
    // leader election
        l_prepare_req=20,
        l_prepare_ack=21,
        l_accept_req=22,
        l_accept_ack=23,
        l_commit_req=24, //used when there is a final leader
        l_commit_ack=25, // used when
}paxos_msg_code;

#endif
