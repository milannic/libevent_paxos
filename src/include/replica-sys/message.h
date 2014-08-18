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

#include "../util/common-header.h"

typedef enum sys_msg_code_t{
    ping_req = 0,
    ping_ack = 1,
    request_submit = 2,
    request_submit_reply = 3,
    request_check = 4,
    app_info = 5,
    client_sync_req = 11,
    client_sync_ack = 12,
}sys_msg_code;

typedef enum req_sub_code_t{
    SUB_SUCC = 0,
    NO_LEADER = 1,
    IN_ERROR = 2,
    NO_RECORD = 3,
    ON_GOING = 4,
    FORFEITED = 5,
    FINISHED = 6,
}req_sub_code;

typedef struct sys_msg_t{
    sys_msg_code code;
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

typedef struct request_submit_msg_t{
    size_t data_size;
    char data[0];
}__attribute__((packed))req_sub_msg;
#define REQ_SUB_SIZE(M) (((M)->data_size)+sizeof(req_sub_msg))

typedef struct request_submit_reply_msg_t{
    req_sub_code code;
    view_stamp req_id;
}__attribute__((packed))req_sub_rep_msg;
#define REQ_SUB_REP_SIZE(M) (sizeof(req_sub_rep_msg))

typedef struct request_check_msg_t{
    view_stamp req_id;
}__attribute__((packed))req_check_msg;
#define REQ_CHECK_SIZE(M) (sizeof(req_check_msg))

typedef struct client_sync_req_msg_t{
    int padding;
}__attribute__((packed))client_sync_req_msg;
#define CLIENT_SYNC_REQ_SIZE(M) (sizeof(client_sync_req_msg))

typedef struct client_sync_ack_msg_t{
    view cur_view;
}__attribute__((packed))client_sync_ack_msg;
#define CLIENT_SYNC_ACK_SIZE(M) (sizeof(client_sync_ack_msg))

//sys_msg* package_sys_msg(sys_msg_code,int,void*);
sys_msg* build_ping_req(int,view*);
sys_msg* build_ping_ack(int,view*);
sys_msg* build_client_sync_ack(view*);
sys_msg* build_request_submit_reply_msg(req_sub_code,view_stamp*);
#endif
