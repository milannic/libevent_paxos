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
#include "../util/common-header.h"

struct node_t;
struct consensus_component_t;

typedef void (*up_call)(struct node_t*,size_t,void*,int);
typedef void (*user_cb)(size_t data_size,void* data);

typedef enum con_role_t{
    LEADER = 0,
    SECONDARY = 1,
}con_role;

struct consensus_component_t* init_consensus_comp(struct node_t*,uint32_t,
        const char*,int,
        view*,user_cb,up_call);

int submit_request(struct consensus_component_t*,size_t,void*,view_stamp*);

int make_progress(struct consensus_component_t*);

int look_up_request(struct consensus_component_t*,view_stamp);

view_stamp get_higghest_seen_req(struct consensus_component_t*);

void update_role(struct consensus_component_t*);

#endif
