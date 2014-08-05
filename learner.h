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

#include "common-header.h"

struct node_t;
typedef void(*l_callback)(int data_size,void*data);

typedef void(*req_missing_record)(struct node_t* l_node,view_stamp missing_id);

typedef struct learner_t{

    view_stamp cur_view_stamp;
    view_stamp highest_committed_view_stamp;
    struct node_t* l_node;    
    // callback function, one is to deliver committed record to the user application
    l_callback l_cb;
    // one is to let its replica node to request missing records from other nodes;
    req_missing_record l_missing_cb;
    // database
    const char* db_name;
    //database_db interface
    
}learner;

#endif
