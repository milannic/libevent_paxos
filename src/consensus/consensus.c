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
#include "../include/consensus/consensus-msg.h"
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


//static function declaration
//

static view_stamp get_next_view_stamp(consensus_component*);

static int leader_handle_submit_req(struct consensus_component_t*,
        size_t,void*,view_stamp*);

static void handle_accept_req(consensus_component* ,size_t ,void* );
static void handle_accept_ack(consensus_component* ,size_t ,void* );
static void handle_missing_req(consensus_component* ,size_t ,void* );
static void handle_missing_ack(consensus_component* ,size_t ,void* );
static void handle_force_exec(consensus_component* ,size_t ,void* );

static void* build_accept_req(consensus_component* ,size_t ,void*, view_stamp*);
static void* build_accept_ack(consensus_component* ,size_t ,void* );
static void* build_missing_req(consensus_component* ,size_t ,void* );
static void* build_missing_ack(consensus_component* ,size_t ,void* );
static void* build_force_exec(consensus_component* ,size_t ,void* );


//public interface method

view_stamp get_higghest_seen_req(consensus_component* comp){
    return comp->highest_seen_vs;
}

void consensus_handle_msg(struct consensus_component_t* comp,
        size_t data_size,void* data){
    consensus_msg_head *head = data;
    switch(head->msg_type){
        case ACCEPT_REQ:
            handle_accept_req(comp,data_size,data);
            break;
        case ACCEPT_ACK:
            handle_accept_ack(comp,data_size,data);
            break;
        case MISSING_REQ:
            handle_missing_req(comp,data_size,data);
            break;
        case MISSING_ACK:
            handle_missing_ack(comp,data_size,data);
            break;
        case FORCE_EXEC:
            handle_force_exec(comp,data_size,data);
            break;
        default:
            paxos_log("Unknown Type of Message %d\n",
                    head->msg_type);
            break;
    }

};

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
    next_vs.req_id = (comp->highest_seen_vs.req_id+1);
    return next_vs;
};
static void view_stamp_inc(consensus_component* comp){
    comp->highest_seen_vs.req_id++;
    return;
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
    accept_req* msg = build_accept_req(comp,data_size,data,vs);
    if(NULL==msg){
        goto handle_submit_req_exit;
    }
    comp->uc(comp->my_node,ACCEPT_REQ_SIZE(msg),msg,-1);
    free(msg);
    view_stamp_inc(comp);
    ret = 0;
handle_submit_req_exit: 
    // no need to care about database, every time we will override it.
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


static void handle_accept_req(consensus_component* comp,size_t data_size,void* data){
    return;
};
static void handle_accept_ack(consensus_component* comp,size_t data_size,void* data){
    return;
};
static void handle_missing_req(consensus_component* comp,size_t data_size,void* data){
    return;
};
static void handle_missing_ack(consensus_component* comp,size_t data_size,void* data){
    return;
};
static void handle_force_exec(consensus_component* comp,size_t data_size,void* data){
    return;
};

static void* build_accept_req(consensus_component* comp,
        size_t data_size,void* data,view_stamp* vs){
    accept_req* msg = (accept_req*)malloc(sizeof(accept_req)+data_size);
    if(NULL==msg){
        goto build_acc_req_exit;
    }
    msg->node_id = comp->node_id;
    msg->req_canbe_exed = comp->highest_to_commit_vs;
    msg->data_size = data_size;
    msg->head.msg_type = ACCEPT_REQ;
    msg->msg_vs = *vs;
    memcpy(msg->data,data,data_size);
build_acc_req_exit:
    return msg;
};
static void* build_accept_ack(consensus_component* comp,size_t data_size,void* data){
    return NULL;
};
static void* build_missing_req(consensus_component* comp,size_t data_size,void* data){
    return NULL;
};
static void* build_missing_ack(consensus_component* comp,size_t data_size,void* data){
    return NULL;
};
static void* build_force_exec(consensus_component* comp,size_t data_size,void* data){
    return NULL;
};

