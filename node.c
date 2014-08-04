/*
 * =====================================================================================
 *
 *       Filename:  node.c
 * *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/31/2014 02:38:45 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */
#include "node.h"
#include "node_config.h"

void connection_free(connection* con){
    if(NULL!=con){
        if(NULL!=con->my_buff_event){
            bufferevent_disable(con->my_buff_event,EV_READ|EV_WRITE);
            bufferevent_free(con->my_buff_event);
        }
        if(NULL!=con->peer_address){
            free(con->peer_address);
        }
        close(con->sock_id);
        free(con);
    }
}

static void peer_node_on_read(struct bufferevent* bev,void* arg){return;};

static void peer_node_on_event(struct bufferevent* bev,short what,void* arg){
    peer* peer_node = (peer*)arg;
    if(what&BEV_EVENT_CONNECTED){
        paxos_log("Connected to Node %d\n",peer_node->peer_id);
        peer_node->active = 1;
    }else if(what & BEV_EVENT_EOF || what & BEV_EVENT_ERROR){
        int err = EVUTIL_SOCKET_ERROR();
		paxos_log("%s (%d)\n",evutil_socket_error_to_string(err),peer_node->peer_id);
        bufferevent_free(bev);
        peer_node->active = 0;
        event_add(peer_node->reconnect,&reconnect_timeval);
    }
};


static void connect_peer(peer* peer_node){
    connection* peer_connection = peer_node->peer_connection;
    peer_connection->my_buff_event = bufferevent_socket_new(peer_node->base,-1,BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(peer_connection->my_buff_event,peer_node_on_read,NULL,peer_node_on_event,peer_node);
    bufferevent_enable(peer_connection->my_buff_event,EV_READ|EV_WRITE|EV_PERSIST);
    bufferevent_socket_connect(peer_connection->my_buff_event,(struct sockaddr*)peer_connection->peer_address,peer_connection->sock_len);
};

static void peer_node_on_timeout(int fd,short what,void* arg){
    connect_peer((peer*)arg);
};

void connect_peers(node* my_node){
    for(int i=0;i<my_node->group_size;i++){
        if(i!=my_node->node_id){
            peer* peer_node = my_node->peer_pool+i;
            peer_node->reconnect = evtimer_new(peer_node->base,peer_node_on_timeout,peer_node);
            connect_peer(peer_node);
        }
    }
}

int initialize_node(node* my_node){
    int flag = 0;
    connect_peers(my_node);
    goto initialize_node_exit;
initialize_node_exit:
        return flag;
}
