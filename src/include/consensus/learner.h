/*
 * =====================================================================================
 *
 *       Filename:  learner.h
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/05/2014 02:27:59 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */

#ifndef LEARNER_H
#define LEARNER_H

#include "../util/common-header.h"

struct consensus_component_t;

typedef void(*learner_deliever_callback)(int,void*);

typedef void(*learner_missing_req_callback)(struct consensus_component_t*,view_stamp);

typedef void(*learner_missing_ack_callback)(struct consensus_component_t*,view_stamp,size_t,void*);

typedef struct learner_t{

    view_stamp cur_view_stamp;
    view_stamp highest_committed_view_stamp;

    view* cur_view;
    struct node_t* l_node;    
    learner_deliever_callback l_d_cb;
    learner_missing_req_callback l_m_r_cb;
    learner_missing_ack_callback l_m_a_cb;
    // database
    const char* db_name;
    //database_db interface
}learner;

learner* initialize_learner(view*,view_stamp*,const
        char*,learner_deliever_callback,learner_missing_req_callback,
        learner_missing_ack_callback);

void learner_handle_accept_req(learner*,view_stamp,size_t,void*);
void learner_handle_missing_req(learner*,view_stamp);

#endif
