/*
 * =====================================================================================
 *
 *       Filename:  proxy.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  09/14/2014 11:20:55 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef PROXY_H
#define PROXY_H

#include "../util/common-header.h"
#include "../rsm-interface.h"
#include "../replica-sys/replica.h"
#include "uthash.h"
#include <pthread.h>


// hash_key type def
typedef uint64_t hk_t;
typedef uint64_t sec_t;
typedef uint16_t nid_t;
typedef uint16_t nc_t;

// record number
typedef uint64_t rec_no_t;
typedef uint64_t flag_t;


// hash table of socket pair
typedef struct socket_pair_t{
    hk_t pairt_key;
    struct bufferevent* p_s;
    struct bufferevent* p_c;
    // hash pair handle
    UT_hash_handle hh;
}socket_pair;

typedef struct proxy_address_t{
    struct sockaddr_in p_addr;
    size_t p_sock_len;
    struct sockaddr_in c_addr;
    size_t c_sock_len;
    struct sockaddr_in s_addr;
    size_t s_sock_len;
}proxy_address;


typedef struct proxy_node_t{
    // socket pair
    nid_t node_id; 
    nc_t pair_count;
    socket_pair* hash_map;
    proxy_address sys_addr;

    // libevent part
    struct event_base* base;
    struct evconnlistener* listener;

    // DMT part
    // in the loop of libevent logical, we must give a way to periodically
    // invoke our actively working function
    struct event* do_action;

    // consensus module
    struct node_t* con_node;
    struct bufferevent* con_conn;

    char* db_name;

}proxy_node;


#endif
