/*
 * =====================================================================================
 *
 *       Filename:  consensus.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/06/2014 02:09:10 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */
#ifndef CONSENSUS_H

#define CONSENSUS_H
#include "common-header.h"

struct node_t;
struct consensus_component_t;

typedef void (*net_up_call)(struct node_t,size_t,void*);

typedef enum con_role_t{
    LEADER = 0,
    SECONDARY = 1,
}con_role;

struct consensus_component_t* initialize_consensus_comp(void(*user_cb)(int
            data_size,void* data),net_up_call up_cb,const char* db_name,int
        group_size,view*,view_stamp*);
void update_role(struct consensus_component_t*,con_role,view);
void deliever_msg(struct consensus_component_t*,size_t,void*);

#endif
