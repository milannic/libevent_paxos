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

typedef enum consensus_msg_code_t{
        // leader election
        ACCEPT_REQ=0,
        ACCEPT_ACK=1,
        MISSING_REQ=2,
        MISSING_ACK=3,
        FORCE_EXEC=4,
}con_code;

typedef struct consensus_msg_head_t{
    con_code msg_type;
}consensus_msg_head;

typedef struct accept_req_t{
    consensus_msg_head head;
    view_stamp msg_vs;
    view_stamp req_canbe_exed;
    uint32_t node_id;
    size_t data_size;
    char data[0];
}__attribute__((packed))accept_req;
#define ACCEPT_REQ_SIZE(M) (sizeof(accept_req)+(M->data_size));

typedef struct accept_ack_t{
    consensus_msg_head head;
    view_stamp msg_vs;
    uint32_t node_id;
}accept_ack;
#define ACCEPT_ACK_SIZE(M) (sizeof(accept_ack))

typedef struct missing_req_t{
    consensus_msg_head head;
    view_stamp missing_vs;
}missing_req;
#define MISSING_REQ_SIZE(M) (sizeof(missing_req))

typedef struct missing_ack_t{
    consensus_msg_head head;
    view_stamp missing_vs;
    size_t data_size;
    char data[0];
}__attribute__((packed))missing_ack;
#define MISSING_ACK_SIZE(M) (sizeof(missing_ack)+(M->data_size))

typedef struct force_exec_t{
    consensus_msg_head head;
    view_stamp missing_vs;
}force_exec;
#define FORCE_EXEC_SIZE(M) (sizeof(force_exec))

#endif