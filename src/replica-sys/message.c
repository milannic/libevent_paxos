/*
 * =====================================================================================
 *
 *       Filename:  message.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/11/2014 03:42:30 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */

#include "../include/replica-sys/message.h"

sys_msg* package_sys_msg(sys_msg_code code,int data_size,void* data){
    sys_msg* msg = (sys_msg*)malloc(SYS_MSG_SIZE(data_size));
    if(NULL!=msg){
        msg->code= code; 
        msg->data_size = data_size;
        memcpy(msg->data,data,data_size);
    }
    return msg;
}

sys_msg* build_ping_req(int node_id, view* cur_view){
    ping_req_msg* ping_msg = (ping_req_msg*)malloc(PING_REQ_SIZE(ping_msg));
    if(NULL==ping_msg){
        return NULL;
    }
    ping_msg->node_id = node_id;
    ping_msg->view.view_id = cur_view->view_id;
    ping_msg->view.leader_id = cur_view->leader_id;
    gettimeofday(&ping_msg->timestamp,NULL);
    sys_msg* msg=package_sys_msg(ping_req,PING_REQ_SIZE(ping_msg),ping_msg);
    free(ping_msg);
    return msg;
}
sys_msg* build_ping_ack(int node_id,view* cur_view){
    ping_ack_msg* ping_msg = (ping_ack_msg*)malloc(PING_ACK_SIZE(ping_msg));
    if(NULL==ping_msg){
        return NULL;
    }
    ping_msg->node_id = node_id;
    ping_msg->view.view_id = cur_view->view_id;
    ping_msg->view.leader_id = cur_view->leader_id;
    gettimeofday(&ping_msg->timestamp,NULL);
    sys_msg* msg=package_sys_msg(ping_ack,PING_ACK_SIZE(ping_msg),ping_msg);
    free(ping_msg);
    return msg;
}
