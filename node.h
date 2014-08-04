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
#include "common-header.h"

extern struct timeval reconnect_timeval;
extern struct timeval ping_timeval;
extern int heart_beat_threshold;

typedef struct connection_t{
// connection info

    int sock_id;
    struct sockaddr_in* peer_address;
    size_t sock_len;
// libevent part
    struct bufferevent* my_buff_event;
    struct event_base* base;

}connection;
void connection_free(connection* con);


typedef struct peer_t{
    int peer_id;
    int active;
    struct node_t* my_node;
    connection* peer_connection;
    struct event_base* base;
    struct event* reconnect;
}peer;

typedef enum node_state_code_t{
    NODE_ACTIVE=0,
    NODE_INACTIVE=1,
}node_state_code;

typedef struct node_t{

    int node_id;
    view cur_view;
    node_state_code state;
    struct sockaddr_in my_address;
    int group_size;
    peer* peer_pool;

    // libevent part
    struct event_base* base;
    struct event* ev_ping_leader;

    char* db_name;
    //database* my_db
}node;


void connect_peers(node* my_node);
int initialize_node(node* my_node);

#endif
