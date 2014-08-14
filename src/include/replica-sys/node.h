/*
 * =====================================================================================
 *
 *       Filename:  server.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/23/2014 03:49:55 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef SERVER_H
#define SERVER_H
#include "../util/common-header.h"
#include "../consensus/consensus.h"
#include "message.h"

typedef struct peer_t{
    int peer_id;
    int active;

    int sock_id;
    struct sockaddr_in* peer_address;
    size_t sock_len;

    struct node_t* my_node;

    struct bufferevent* my_buff_event;
    struct event_base* base;
    struct event* reconnect; // reconnect the peer
}peer;

typedef enum node_state_code_t{
    NODE_ACTIVE=0,
    NODE_INACTIVE=1,
}node_state_code;

typedef struct node_config_t{
    struct timeval reconnect_timeval;
    struct timeval ping_timeval;
    int heart_beat_threshold;
    int max_connections;
}node_config;


typedef void (*msg_handler)(struct node_t*,struct evbuffer*);

typedef struct node_t{

    uint32_t node_id;

    view cur_view;
    view_stamp highest_committed_view_stamp;

    node_state_code state;
    struct sockaddr_in my_address;
    uint32_t group_size;
    peer* peer_pool;

    //struct consensus_component_t* consensus_comp;

    //config
    node_config config;
    // libevent part
    struct evconnlistener* listener;
    struct event_base* base;
    msg_handler msg_cb;
    struct event* ev_leader_ping;
    struct timeval last_ping_msg;

    //databse part
    char* db_name;
    //database* my_db
}node;

#endif
