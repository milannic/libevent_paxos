/*
 * =====================================================================================
 *
 *       Filename:  consensus.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/06/2014 02:13:09 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Milannic (), milannic.cheng.liu@gmail.com
 *   Organization:  
 *
 * =====================================================================================
 */

#include "../include/consensus/consensus.h"
#include "../include/db/db-interface.h"

static struct timeval request_repeat_timeout = {2,0};

typedef struct request_record_t{
    struct timeval created_time; // data created timestamp
    uint64_t bit_map; // now we assume the maximal replica group size is 64;
    char is_closed;   // whether the request has been closed
    size_t data_size; // data size
    char data[0];     // real data
}__attribute__((packed))request_record;
#define REQ_RECORD_SIZE(M) (sizeof(request_record)+(M->data_size))

typedef struct consensus_component_t{
    con_role my_role;
    uint32_t node_id;

    uint32_t group_size;
    struct node_t* my_node;

    view* cur_view;
    view_stamp highest_seen_vs; 
    view_stamp highest_to_commit_vs;
    view_stamp highest_committed_vs;

    db* db_ptr;
    up_call uc;
    user_cb ucb;
}consensus_component;

static int leader_handle_submit_req(struct consensus_component_t*,
        size_t,void*,view_stamp*);

static view_stamp get_next_view_stamp(consensus_component*);


//public interface method

view_stamp get_higghest_seen_req(struct consensus_component_t* comp){
    return comp->highest_seen_vs;
}

consensus_component* init_consensus_comp(struct node_t* node,uint32_t node_id,
        const char* db_name,int group_size,
        view* cur_view,user_cb u_cb,up_call uc){
    consensus_component* comp = (consensus_component*)
        malloc(sizeof(consensus_component));
    if(NULL!=comp){
        comp->db_ptr = initialize_db(db_name,0);
        if(NULL==comp->db_ptr){
            goto consensus_error_exit;
        }
        comp->my_node = node;
        comp->node_id = node_id;
        comp->group_size = group_size;
        comp->cur_view = cur_view;
        if(comp->cur_view->leader_id == comp->node_id){
            comp->my_role = LEADER;
        }else{
            comp->my_role = SECONDARY;
        }
        comp->ucb = u_cb;
        comp->uc = uc;
        comp->highest_seen_vs.req_id = 0;
        comp->highest_seen_vs.view_id = comp->cur_view->view_id;
        comp->highest_committed_vs.view_id = 0; 
        comp->highest_committed_vs.req_id = 0; 
        comp->highest_to_commit_vs.view_id = 0;
        comp->highest_to_commit_vs.view_id = 0;
        goto consensus_init_exit;

    }
consensus_error_exit:
    if(comp!=NULL){
        if(NULL!=comp->db_ptr){
            close_db(comp->db_ptr,0);
        }
        free(comp);
    }
consensus_init_exit:
    return comp;
}

int submit_request(struct consensus_component_t* comp,
        size_t data_size,void* data,view_stamp* vs){
    if(LEADER==comp->my_role){
       return leader_handle_submit_req(comp,data_size,data,vs);
    }else{
        return 2;
    }
}

void update_role(struct consensus_component_t* comp){
    if(comp->cur_view->leader_id!=comp->node_id){
        comp->my_role = SECONDARY;
    }else{
        comp->my_role = LEADER;
    }
    comp->highest_seen_vs.view_id = comp->cur_view->view_id;
    comp->highest_seen_vs.req_id = 0;
    return;
}



//static method area

static view_stamp get_next_view_stamp(consensus_component* comp){
    view_stamp next_vs;
    next_vs.view_id = comp->highest_seen_vs.view_id;
    next_vs.req_id = (++comp->highest_seen_vs.req_id);
    return next_vs;
};


static int leader_handle_submit_req(struct consensus_component_t* comp,
        size_t data_size,void* data,view_stamp* vs){
    int ret = 1;
    view_stamp next = get_next_view_stamp(comp);
    vs->view_id = next.view_id;
    vs->req_id = next.req_id;
    uint64_t recordno = vstol(vs);
    request_record* record_data = 
        (request_record*)malloc(data_size+sizeof(request_record));
    gettimeofday(&record_data->created_time,NULL);
    record_data->bit_map = (1<<comp->node_id);
    memcpy(record_data->data,data,data_size);
    if(store_record(comp->db_ptr,recordno,data_size,data)){
        goto handle_submit_req_exit;
    }    
    ret = 0;
handle_submit_req_exit: 
    if(record_data!=NULL){
        free(record_data);
    }
    return ret;
}

static void update_record(request_record* record,uint32_t node_id){
    record->bit_map = (record->bit_map | (1<<node_id));
    return;
}

static int reached_quorum(request_record* record,int group_size){
    // this may be compatibility issue 
    if(__builtin_popcountl(record->bit_map)>=((group_size/2)+1)){
        return 1;
    }else{
        return 0;
    }
}



