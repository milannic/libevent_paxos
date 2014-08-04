/*
 * =====================================================================================
 *
 *       Filename:  config.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  07/31/2014 02:21:17 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#include "config.h"

int read_configuration_file(node* cur_node,const char* config_file){
    config_t node_config;
    config_init(&node_config);

    if(!config_read_file(&node_config,config_file)){
        goto goto_config_error;
    }
    int group_size;
    if(!config_lookup_int(&node_config,"group_size",&group_size)){
        goto goto_config_error;
    }

    cur_node->group_size = group_size;
    cur_node->peer_pool = (peer*)malloc(group_size*sizeof(peer));
    if(NULL==cur_node->peer_pool){
        goto goto_config_error;
    }

    if(group_size<=cur_node->node_id){
        paxos_log("invalid node id\n");
        goto goto_config_error;
    }

    config_setting_t *nodes_setting;
    nodes_setting = config_lookup(&node_config,"nodes_config");

    if(NULL==nodes_setting){
        paxos_log("cannot find nodes settings \n");
        goto goto_config_error;
    }    

    config_setting_t *net_address;
    debug_log("the length is %d\n",config_setting_length(nodes_setting));
    net_address= config_setting_get_member(nodes_setting,"net_address");

    if(NULL==nodes_setting){
        paxos_log("cannot find net address section \n");
        goto goto_config_error;
    }
    debug_log("the group size is %d\n",group_size);
    for(int i=0;i<group_size;i++){ 
        config_setting_t *node_address = config_setting_get_elem(net_address,i);
        if(NULL==node_address){
            paxos_log("cannot find current node's address\n");
            goto goto_config_error;
        }

        const char* my_ipaddr;
        int my_port;
        if(!config_setting_lookup_string(node_address,"ip_address",&my_ipaddr)){
            goto goto_config_error;
        }
        if(!config_setting_lookup_int(node_address,"port",&my_port)){
            goto goto_config_error;
        }
        cur_node->peer_pool[i].my_node = cur_node;
        cur_node->peer_pool[i].peer_id = i;
        cur_node->peer_pool[i].base = cur_node->base;
        cur_node->peer_pool[i].reconnect = NULL;
        cur_node->peer_pool[i].peer_connection = (connection*) malloc(sizeof(connection));
        cur_node->peer_pool[i].peer_connection->base = cur_node->base;
        cur_node->peer_pool[i].peer_connection->sock_id = -1;
        cur_node->peer_pool[i].peer_connection->my_buff_event = NULL;
        cur_node->peer_pool[i].peer_connection->peer_address = (struct sockaddr_in*)malloc(sizeof(struct sockaddr_in));
        cur_node->peer_pool[i].peer_connection->sock_len = sizeof(struct sockaddr_in);
        cur_node->peer_pool[i].peer_connection->peer_address->sin_family = AF_INET;
        inet_pton(AF_INET,my_ipaddr,&cur_node->peer_pool[i].peer_connection->peer_address->sin_addr);
        cur_node->peer_pool[i].peer_connection->peer_address->sin_port = htons(my_port);

        if(i==cur_node->node_id){
            cur_node->my_address.sin_port = htons(my_port);
            cur_node->my_address.sin_family = AF_INET;
            inet_pton(AF_INET,my_ipaddr,&cur_node->my_address.sin_addr);
        }
        debug_log("the current node no is %d\n",i);
        debug_log("the ip address is %s:%d\n",my_ipaddr,my_port);
    }


    config_destroy(&node_config);
    return 0;

goto_config_error:
    paxos_log("%s:%d - %s\n", config_error_file(&node_config), config_error_line(&node_config), config_error_text(&node_config));
    config_destroy(&node_config);
    return -1;
};
