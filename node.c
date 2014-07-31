/*
 * =====================================================================================
 *
 *       Filename:  node.c
 *
 *    Description:  
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
