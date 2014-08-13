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


typedef enum sys_msg_code_t{
    ping_req = 0,
    ping_ack = 1,
    request_submit_req = 2,
    request_submit_ack = 3,
    app_comm_msg = 5,
}sys_msg_code;

typedef struct sys_msg_t{
    sys_msg_code type;
    size_t data_size;
    char data[0];
}__attribute__((packed))sys_msg;
#define SYS_MSG_SIZE(size) ((size) + sizeof(sys_msg))


typedef struct ping_req_msg_t{
    int node_id;
    view view;
    struct timeval timestamp;
}__attribute__((packed))ping_req_msg;
#define PING_REQ_SIZE(M) (sizeof(ping_req_msg))

typedef struct ping_ack_msg_t{
    int node_id;
    view view;
    struct timeval timestamp;
}__attribute__((packed))ping_ack_msg;
#define PING_ACK_SIZE(M) (sizeof(ping_ack_msg))

sys_msg* package_sys_msg(sys_msg_code,int,void*);
sys_msg* build_ping_req(int,view*);
sys_msg* build_ping_ack(int,view*);

#endif
