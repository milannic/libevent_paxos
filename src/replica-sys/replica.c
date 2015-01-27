#include "../include/util/common-header.h"
#include "../include/replica-sys/node.h"
#include "../include/config-comp/config-comp.h"
#include <sys/stat.h>

#define forlessp(var,start,end) for((var) = (start);i<(end);i++)
#define foreqp(var,start,end) for((var) = (start);i<=(end);i++)
#define forlessm(var,start,end) for((var) = (start);i>(end);i--)
#define foreqm(var,start,end) for((var) = (start);i>=(end);i--)

#define CHECK_EXIT do{if(exit_flag){\
    SYS_LOG(my_node,"Component Received TERMINATION SIGNAL,NOW QUIT.\n")\
    event_base_loopexit((my_node)->base,NULL);}}while(0);

//for debug
//
struct timeval first_proposer_lele = {1,0};
struct timeval wait_for_net_lele = {3,0};
struct timeval stop_by_other_lele = {10,0};

static int exit_flag = 0;


int max_waiting_connections = MAX_ACCEPT_CONNECTIONS; 
static unsigned current_connection = 3;

static void usage(){
    err_log("Usage : -n NODE_ID\n");
    err_log("        -m [sr] Start Mode seed|recovery\n");
    err_log("        -c path path to configuration file\n");
}

//msg handler
static void replica_on_read(struct bufferevent*,void*);
static void replica_on_error_cb(struct bufferevent*,short,void*);

//signal handler
static void node_singal_handler(evutil_socket_t fid,short what,void* arg);
static void node_sys_sig_handler(int sig);

// consensus part
static void send_for_consensus_comp(node*,size_t,void*,int);


//concrete message
static void handle_ping_ack(node* ,ping_ack_msg* );
static void handle_ping_req(node* ,ping_req_msg* );


//listener
static void replica_on_accept(struct evconnlistener* listener,evutil_socket_t
        fd,struct sockaddr *address,int socklen,void *arg);

//peer connection
static void peer_node_on_event(struct bufferevent* bev,short ev,void* arg);
static void peer_node_on_read(struct bufferevent*,void*);
static void connect_peer(peer*);
static void peer_node_on_timeout(int fd,short what,void* arg);
static void connect_peers(node*);


//ping part
static void leader_ping_period(int fd,short what,void* arg);
static void expected_leader_ping_period(int fd,short wat,void* arg);
static int initialize_leader_ping(node*);
static int initialize_expect_ping(node*);

//make progress
static int initialize_leader_make_progress(node* my_node);
static void make_progress_on(int fd,short what,void* arg);

//view change
static void handle_leader_election_msg(node*,leader_election_msg*);
static void initialize_leader_election(node*);
static void initialize_leader_election_mod(node*);
static void free_leader_election_mod(node*,lele_mod*);
static void leader_election_on_timeout(int fd,short what,void* arg);
static int proposer_check_prepare(node*,lele_mod*,lele_msg*);
static void leader_election_proposer_do(node*,lele_mod*,lele_msg*);
static void leader_election_proposer_phase_two(node*,lele_mod*,lele_msg*);
static void leader_election_acceptor_do(node*,lele_mod*,lele_msg*);
static int learner_check_accept(node*,lele_mod*,lele_msg*);
static void leader_election_learner_do(node*,lele_mod*,lele_msg*);
static void leader_election_finalize(node*,lele_mod*,lele_msg*);
static void update_view(node*,view*);
static void become_leader(node*);
static void giveup_leader(node*);
static void replica_sync(node*);

//free related function
static void free_peers(node*);
static int free_node(node*);

//helper function
static int isLeader(node*);
static int send_to_other_nodes(node*,void*,size_t,char*);
static int send_to_one_node(node*,node_id_t,void*,size_t,char*);



static void node_sys_sig_handler(int sig){
    exit_flag = 1;
}

//implementation level

static void node_singal_handler(evutil_socket_t fid,short what,void* arg){
    node* my_node = arg;
    if(what&EV_SIGNAL){
        SYS_LOG(my_node,"Node %d Received Kill Singal.Now Quit.\n",my_node->node_id);
    }
    event_base_loopexit(my_node->base,NULL);
}



static int send_to_other_nodes(node* my_node,void* data,size_t data_size,char* comment){
    uint32_t i;
    forlessp(i,0,my_node->group_size){
        if(i!=my_node->node_id && my_node->peer_pool[i].active){
            struct bufferevent* buff = my_node->peer_pool[i].my_buff_event;
            bufferevent_write(buff,data,data_size);
            SYS_LOG(my_node,
                    "Send %s To Node %u\n",comment,i);
        }
    }
    return 0;
}


static int send_to_one_node(node* my_node,node_id_t target,void* data,size_t data_size,char* comment){
    //if(target!=(int)my_node->node_id&&my_node->peer_pool[target].active){
    if(my_node->peer_pool[target].active){
        struct bufferevent* buff = my_node->peer_pool[target].my_buff_event;
        bufferevent_write(buff,data,data_size);
        SYS_LOG(my_node,
                "Send %s To Node %u.\n",comment,target);
    }
    return 0;
}

static void peer_node_on_read(struct bufferevent* bev,void* arg){return;};

static void peer_node_on_event(struct bufferevent* bev,short ev,void* arg){
    peer* peer_node = (peer*)arg;
    node* my_node = peer_node->my_node;
    if(ev&BEV_EVENT_CONNECTED){
        SYS_LOG(my_node,"Connected to Node %d\n",peer_node->peer_id);
        peer_node->active = 1;
    }else if((ev & BEV_EVENT_EOF )||(ev&BEV_EVENT_ERROR)){
        if(peer_node->active){
            peer_node->active = 0;
            SYS_LOG(my_node,"Lost Connection With Node %d \n",peer_node->peer_id);
        }
        peer_node->my_buff_event = NULL;
        int err = EVUTIL_SOCKET_ERROR();
		SYS_LOG(my_node,"%s (%d)\n",evutil_socket_error_to_string(err),peer_node->peer_id);
        bufferevent_free(bev);
        event_add(peer_node->reconnect,&my_node->config.reconnect_timeval);
    }
    CHECK_EXIT;
    return;
};


static void connect_peer(peer* peer_node){
    node* my_node = peer_node->my_node;
    peer_node->my_buff_event = bufferevent_socket_new(peer_node->base,-1,BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(peer_node->my_buff_event,peer_node_on_read,NULL,peer_node_on_event,peer_node);
    bufferevent_enable(peer_node->my_buff_event,EV_READ|EV_WRITE|EV_PERSIST);
    bufferevent_socket_connect(peer_node->my_buff_event,(struct sockaddr*)peer_node->peer_address,peer_node->sock_len);
    CHECK_EXIT;
};

static void peer_node_on_timeout(int fd,short what,void* arg){
    peer* p = arg;
    node* my_node = p->my_node;
    connect_peer(p);
    CHECK_EXIT;
};


static void connect_peers(node* my_node){
    for(uint32_t i=0;i<my_node->group_size;i++){
        if(i!=my_node->node_id){
            peer* peer_node = my_node->peer_pool+i;
            peer_node->reconnect = evtimer_new(peer_node->base,peer_node_on_timeout,peer_node);
            connect_peer(peer_node);
        }
    }
    CHECK_EXIT;
}


static void lost_connection_with_leader(node* my_node){
    SYS_LOG(my_node,"Node %u Lost Connection With The Leader\n",
            my_node->node_id);
    initialize_leader_election(my_node);
    SYS_LOG(my_node,"Node %u Will Start A Leader Election\n",
            my_node->node_id);
    CHECK_EXIT
    return;
}

static void expected_leader_ping_period(int fd,short what,void* arg){
    node* my_node = arg;
    if(my_node->node_id==my_node->cur_view.leader_id){
        if(NULL!=my_node->ev_leader_ping){
            event_free(my_node->ev_leader_ping);
            initialize_leader_ping(my_node);
        }
    }else{
        struct timeval* last = &my_node->last_ping_msg;
        struct timeval cur;
        gettimeofday(&cur,NULL);
        struct timeval temp;
        timeval_add(last,&my_node->config.expect_ping_timeval,&temp);
        if(timeval_comp(&temp,&cur)>=0){
            event_add(my_node->ev_leader_ping,&my_node->config.expect_ping_timeval);
        }else{
            SYS_LOG(my_node,
                    "Node %d Haven't Heard From The Leader\n",
                    my_node->node_id);
            lost_connection_with_leader(my_node);
        }
    }
    CHECK_EXIT
    return;
}


static void leader_ping_period(int fd,short what,void* arg){
    node* my_node = arg; 
    SYS_LOG(my_node,"Leader Tries To Ping Other Nodes\n");
    // at first check whether I am the leader
    if(my_node->cur_view.leader_id!=my_node->node_id){
        if(my_node->ev_leader_ping!=NULL){
            event_free(my_node->ev_leader_ping);
            initialize_expect_ping(my_node);
        }
    }else{
        void* ping_req = build_ping_req(my_node->node_id,&my_node->cur_view);
        if(NULL!=ping_req){
            free(ping_req);
            send_to_other_nodes(my_node,ping_req,PING_REQ_SIZE,"Ping Req Msg");
        }else{
            goto add_ping_event;
        }
        add_ping_event:
        if(NULL!=my_node->ev_leader_ping){
            event_add(my_node->ev_leader_ping,&my_node->config.ping_timeval);
        }
    }
    CHECK_EXIT
    return;
};

static int initialize_leader_ping(node* my_node){
    if(NULL==my_node->ev_leader_ping){
        my_node->ev_leader_ping = evtimer_new(my_node->base,leader_ping_period,(void*)my_node);
        if(my_node->ev_leader_ping==NULL){
            return 1;
        }
    }
    event_add(my_node->ev_leader_ping,&my_node->config.ping_timeval);
    CHECK_EXIT
    return 0;
}

static int initialize_expect_ping(node* my_node){
    if(NULL==my_node->ev_leader_ping){
        my_node->ev_leader_ping = evtimer_new(my_node->base,expected_leader_ping_period,(void*)my_node);
        if(my_node->ev_leader_ping==NULL){
            return 1;
        }
    }
    event_add(my_node->ev_leader_ping,&my_node->config.expect_ping_timeval);
    CHECK_EXIT
    return 0;
}

static void make_progress_on(int fd,short what,void* arg){
    node* my_node = arg; 
    SYS_LOG(my_node,"Leader Tries To Make Progress.\n");
    // at first check whether I am the leader
    if(my_node->cur_view.leader_id!=my_node->node_id){
        if(my_node->ev_make_progress!=NULL){
            event_free(my_node->ev_make_progress);
            initialize_leader_make_progress(my_node);
        }
        goto make_progress_on_exit;
    }
    if(NULL!=my_node->consensus_comp){
        consensus_make_progress(my_node->consensus_comp);
    }
    if(NULL!=my_node->ev_make_progress){
        event_add(my_node->ev_make_progress,&my_node->config.make_progress_timeval);
    }
make_progress_on_exit:
    CHECK_EXIT
    return;
}

static int initialize_leader_make_progress(node* my_node){
    int ret = 0;
    if(NULL==my_node->ev_make_progress){
        my_node->ev_make_progress = evtimer_new(my_node->base,make_progress_on,(void*)my_node);
        if(my_node->ev_make_progress==NULL){
            ret = 1;
            goto initialize_leader_make_progress_exit;
        }
    }
    event_add(my_node->ev_make_progress,&my_node->config.make_progress_timeval);
initialize_leader_make_progress_exit:
    CHECK_EXIT;
    return ret;
}



static void update_view(node* my_node,view* new_view){
    SYS_LOG(my_node,"Node %d Entered Update View\n",
            my_node->node_id);
    int old_leader = isLeader(my_node);
    memcpy(&my_node->cur_view,new_view,sizeof(view));
    int new_leader = isLeader(my_node);
    if(old_leader!=new_leader){
        if(new_leader){
            become_leader(my_node);
        }else{
            giveup_leader(my_node);
        }
    }
    SYS_LOG(my_node,"Node %d 's Current View Changed To %u \n",
        my_node->node_id,my_node->cur_view.view_id);
    CHECK_EXIT
    return;
}

static void become_leader(node* my_node){
    CHECK_EXIT
    return;
}
static void giveup_leader(node* my_node){
}
static void replica_sync(node* my_node){
    return;
}

static void free_peer(peer* peer_node){
    if(NULL!=peer_node){
        if(NULL!=peer_node->my_buff_event){
            bufferevent_free(peer_node->my_buff_event);
        }
        if(NULL!=peer_node->reconnect){
            event_free(peer_node->reconnect);
        }
        if(NULL!=peer_node->peer_address){
            free(peer_node->peer_address);
        }
        free(peer_node);
    }
    return;
}

static void free_peers(node* my_node){
    for(uint32_t i=0;i<my_node->group_size;i++){
        free_peer(my_node->peer_pool+i);
    }
}

static int isLeader(node* my_node){
    CHECK_EXIT;
    return my_node->cur_view.leader_id==my_node->node_id;
}

static int free_node(node* my_node){
    if(my_node->listener!=NULL){
        evconnlistener_free(my_node->listener);
    }
    if(my_node->signal_handler!=NULL){
        event_free(my_node->signal_handler);
    }
    if(my_node->ev_make_progress!=NULL){
        event_free(my_node->ev_make_progress);
    }
    if(my_node->ev_leader_ping!=NULL){
        event_free(my_node->ev_leader_ping);
    }
    if(my_node->base!=NULL){
        event_base_free(my_node->base);
    }
    
    return 0;
}


static void replica_on_error_cb(struct bufferevent* bev,short ev,void *arg){
    node* my_node = arg;
    if(ev&BEV_EVENT_EOF){
        // the connection has been closed;
        // do the cleaning
    }
    CHECK_EXIT
    return;
}

static void replica_on_accept(struct evconnlistener* listener,evutil_socket_t fd,struct sockaddr *address,int socklen,void *arg){
    node* my_node = arg;
    SYS_LOG(my_node, "A New Connection Is Established.\n");
    struct bufferevent* new_buff_event = bufferevent_socket_new(my_node->base,fd,BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(new_buff_event,replica_on_read,NULL,replica_on_error_cb,(void*)my_node);
    bufferevent_enable(new_buff_event,EV_READ|EV_PERSIST|EV_WRITE);
    CHECK_EXIT
    return;
};

// consensus part
static void send_for_consensus_comp(node* my_node,size_t data_size,void* data,int target){
    consensus_msg* msg = build_consensus_msg(data_size,data);
    if(NULL==msg){
        goto send_for_consensus_comp_exit;
    }
    // means send to every node except me
    if(target<0){
        send_to_other_nodes(my_node,msg,
                CONSENSUS_MSG_SIZE(msg),"Consensus Msg");
    }else{
        send_to_one_node(my_node,target,msg,CONSENSUS_MSG_SIZE(msg),"Consensus Msg");
    }
send_for_consensus_comp_exit:
    if(msg!=NULL){
        free(msg);
    }
    CHECK_EXIT
    return;
}

static void handle_ping_ack(node* my_node,ping_ack_msg* msg){
    if(my_node->cur_view.view_id < msg->view.view_id){
        update_view(my_node,&msg->view);
    }else{
        SYS_LOG(my_node,
                "Ignore Ping Ack From Node %u.\n",msg->node_id);
    }
    CHECK_EXIT
}

static void handle_ping_req(node* my_node,ping_req_msg* msg){
    SYS_LOG(my_node,
            "Received Ping Req Msg In Node %u From Node %u.",
        my_node->node_id,msg->node_id);
    if(my_node->cur_view.view_id < msg->view.view_id){
        update_view(my_node,&msg->view);
    }else if(my_node->cur_view.view_id > msg->view.view_id){
        if(my_node->peer_pool[msg->node_id].active){
            void* ping_ack = build_ping_ack(my_node->node_id,&my_node->cur_view);
            if(NULL!=ping_ack){
                send_to_one_node(my_node,msg->node_id,ping_ack,PING_ACK_SIZE,"Lagged Ping Ack Msg");
                free(ping_ack);
            }
        } 
        goto handle_ping_req_exit;
    }
    if(my_node->cur_view.view_id == msg->view.view_id){
        if(!isLeader(my_node)){
            if(timeval_comp(&my_node->last_ping_msg,&msg->timestamp)<0){
                memcpy(&my_node->last_ping_msg,&msg->timestamp,
                        sizeof(struct timeval));
            }
            if(NULL!=my_node->ev_leader_ping){
                evtimer_del(my_node->ev_leader_ping);
                evtimer_add(my_node->ev_leader_ping,&my_node->config.expect_ping_timeval);
            }
        }else{
            // leader should not receive the ping req, otherwise the sender is
            // lagged behind,otherwise the leader is outdated which will be
            // treated as a smaller cur view than what in the msg but when they have 
            // the same view id, which can be ignored
            SYS_LOG(my_node,"Received Ping Req From %u In View %u\n",
                    msg->node_id,msg->view.view_id);
        }
    }
handle_ping_req_exit:
    CHECK_EXIT
    return;
}

static void handle_consensus_msg(node* my_node,consensus_msg* msg){
    SYS_LOG(my_node,"Node %d Received Consensus Message\n",
            my_node->node_id);
    if(NULL!=my_node->consensus_comp){
        consensus_handle_msg(my_node->consensus_comp,msg->header.data_size,(void*)msg+SYS_MSG_HEADER_SIZE);
    }
    CHECK_EXIT
    return;
}

static void handle_request_submit(node* my_node,
        req_sub_msg* msg,struct bufferevent* evb){
    SYS_LOG(my_node,"Node %d Received Consensus Submit Request\n",
            my_node->node_id);
    SYS_LOG(my_node,"The Data Size Is %lu \n",
            msg->header.data_size);
    if(NULL!=my_node->consensus_comp){
        view_stamp return_vs;
        consensus_submit_request(
                my_node->consensus_comp,msg->header.data_size,
                (void*)msg+SYS_MSG_HEADER_SIZE,&return_vs);
    }
    CHECK_EXIT
    return;
}



static void initialize_leader_election(node* my_node){
    DEBUG_ENTER
    SYS_LOG(my_node,"Initialize One Instance Of Leader Election In Node %d.n",
            my_node->node_id);
    my_node->state = NODE_INLELE;
    lele_mod* mod = NULL;
    if(NULL==my_node->election_mod){
        initialize_leader_election_mod(my_node);
    }
    mod = my_node->election_mod;
    if(NULL!=mod){
        mod->next_view = my_node->cur_view.view_id+1;
        mod->next_pnum = my_node->node_id;
        //mod->is_proposer = 1;
        uint32_t i;
        forlessp(i,0,my_node->group_size){
            mod->learner_arr[i].pnum = -1;
            // -1 means the proposer can pick up any value
            mod->proposer_arr[i].content = -1;
            mod->proposer_arr[i].a_pnum = -1;
        }
        evtimer_add(mod->slient_period,&first_proposer_lele);
    }
    DEBUG_LEAVE
    CHECK_EXIT
    return;
}

static void initialize_leader_election_mod(node* my_node){
    DEBUG_ENTER
    my_node->election_mod = (lele_mod*)malloc(LELE_MOD_SIZE);
    lele_mod* mod = my_node->election_mod;
    if(NULL!=mod){
        mod->final_state = 0;
        mod->slient_period = evtimer_new(my_node->base,
                leader_election_on_timeout,(void*)my_node);
        mod->learner_arr = (accepted_record*)malloc(
                my_node->group_size*ACCEPTED_REC_SIZE);
        mod->proposer_arr = (proposer_record*)
            malloc(my_node->group_size*PROPOSER_REC_SIZE);
        if(mod->learner_arr==NULL || mod->proposer_arr==NULL){
            free_leader_election_mod(my_node,mod);
        }
    }
    DEBUG_LEAVE
    return;
}

static void free_leader_election_mod(node* my_node,lele_mod* mod){
    DEBUG_ENTER
    if(mod->learner_arr!=NULL){
        free(mod->learner_arr);
    }
    if(mod->proposer_arr!=NULL){
        free(mod->proposer_arr);
    }
    free(mod);
    my_node->election_mod = NULL;
    DEBUG_LEAVE
    return;
}
static void leader_election_on_timeout(int fd,short what,void* arg){
    DEBUG_ENTER
    node* my_node = (node*)arg;
    assert(NULL!=my_node&&"NODE IS NULL");
    lele_mod* mod = my_node->election_mod;
    assert(NULL!=mod&&"MOD IS NULL");
    evtimer_add(mod->slient_period,&wait_for_net_lele);
    if(mod->final_state){
    }else{
        leader_election_proposer_do(my_node,mod,NULL);
    }
    DEBUG_LEAVE
    return;
}

static int acceptor_update_record(node* my_node){
    db_key_type record_no = my_node->election_mod->next_view;
    acceptor_record* record_data = malloc(ACCEPTOR_REC_SIZE);
    if(NULL==record_data){
        return 1;
    }
    memcpy(record_data,&my_node->election_mod->acceptor,ACCEPTOR_REC_SIZE);
    if(store_record(my_node->db_ptr,sizeof(record_no),&record_no,
                ACCEPTOR_REC_SIZE,record_data)){
        return 1;
    }    
    return 0;
}

static int proposer_check_prepare(node* my_node,lele_mod* mod,lele_msg* ret_msg){
    DEBUG_ENTER
    int g_half_size = my_node->group_size/2;
    ret_msg->pnum = -1;
    ret_msg->content = -1;
    int inc = 0;
    pnum_t origin = mod->next_pnum-my_node->group_size;
    uint32_t i;
    forlessp(i,0,my_node->group_size){
        if(mod->proposer_arr[i].p_pnum == origin){
            if(mod->proposer_arr[i].a_pnum != -1){
                inc++;
            }
            if(mod->proposer_arr[i].a_pnum > ret_msg->pnum){
                ret_msg->content = mod->proposer_arr[i].content;
                ret_msg->pnum = mod->proposer_arr[i].a_pnum;
            }
        }
    }
    ret_msg->pnum = origin;
    DEBUG_LEAVE
    return (inc>=g_half_size);
}

// send accept msg
static void leader_election_proposer_phase_two(node* my_node,lele_mod* mod,lele_msg* msg){
    DEBUG_ENTER
    assert(msg->pnum!=-1&&
            "Now that we come to this place, we must have a valid propose number");
    if(msg->content==-1){
        // we can propose any number to be the next leader
        msg->content = my_node->node_id;
    }
    leader_election_msg* sent_msg = build_lele_msg(
                my_node->node_id,mod,LELE_ACCEPT,msg);
    if(sent_msg!=NULL){
        send_to_other_nodes(my_node,sent_msg,LEADER_ELECTION_MSG_SIZE,"Accept Msg");
        free(sent_msg);
    }
    DEBUG_LEAVE
    return;
}


static void leader_election_proposer_do(node* my_node,lele_mod* mod,lele_msg* msg){
    DEBUG_ENTER
    lele_msg accept_req;
    if(NULL==msg){ // send prepare msg
        leader_election_msg* sent_msg = build_lele_msg(
                my_node->node_id,mod,LELE_PREPARE,NULL);
        mod->next_pnum += my_node->group_size;
        evtimer_del(mod->slient_period);
        evtimer_add(mod->slient_period,&wait_for_net_lele);
        if(sent_msg!=NULL){
            send_to_other_nodes(my_node,sent_msg,LEADER_ELECTION_MSG_SIZE,"Prepare Msg");
            //update itself
            if(mod->acceptor.highest_seen_pnum<sent_msg->vc_msg.pnum){
                mod->acceptor.highest_seen_pnum = sent_msg->vc_msg.pnum;
                mod->proposer_arr[my_node->node_id].p_pnum = sent_msg->vc_msg.pnum;
                mod->proposer_arr[my_node->node_id].a_pnum = mod->acceptor.accepted_pnum;
                mod->proposer_arr[my_node->node_id].content = mod->acceptor.content;
                int ret = 1;
                do{
                    ret = acceptor_update_record(my_node);
                }while(ret);
                // if we have got more than half nodes' prepare ack // but it seems it is not possible
                if(proposer_check_prepare(my_node,mod,&accept_req)){
                    leader_election_proposer_phase_two(my_node,mod,&accept_req);
                }
            }
            free(sent_msg);
        }
    }else{ // then it means we've got the PREPARE_ACK msg;
        assert((msg->type==LELE_PREPARE_ACK)&&"We Get There Just Because We've Got Prepare ACK Msg.\n");
        pnum_t origin = mod->next_pnum-my_node->group_size;
        // we only care about the recent PREPARE_ACK msg
        // and only care about the recent view
        if(msg->tail_data.p_pnum == origin && msg->next_view == mod->next_view){
            mod->proposer_arr[msg->node_id].p_pnum = origin;
            mod->proposer_arr[msg->node_id].content = msg->content;
            mod->proposer_arr[msg->node_id].a_pnum = msg->pnum;
            if(proposer_check_prepare(my_node,mod,&accept_req)){
                leader_election_proposer_phase_two(my_node,mod,&accept_req);
            }
        }
    }
    DEBUG_LEAVE
    return;
}

static void stop_propose(node* my_node,lele_mod* mod){
    if(NULL!=mod){
        evtimer_del(mod->slient_period);
        evtimer_add(mod->slient_period,&stop_by_other_lele);
    }
}


static void leader_election_acceptor_do(node* my_node,lele_mod* mod,lele_msg* msg){
    DEBUG_ENTER
    assert(NULL!=msg&&"We Get Here Because We've Got Some Message.\n");
    leader_election_msg* sent_msg = NULL;
    if(msg->type==LELE_PREPARE){
        //optimization,
        if((msg->node_id)<(my_node->node_id)){
            stop_propose(my_node,mod);
        }
        if(msg->pnum>mod->acceptor.highest_seen_pnum){
            int ret = 1;
            mod->acceptor.highest_seen_pnum = msg->pnum;
            do{
                ret = acceptor_update_record(my_node);
                if(ret){
                    SYS_LOG(my_node,"Database Operation Failed in %s,Try Agagin.\n",
                            __PRETTY_FUNCTION__);
                }
            }while(ret);
            sent_msg = build_lele_msg(my_node->node_id,
                    mod,LELE_PREPARE_ACK,NULL);
            if(NULL!=sent_msg){
                send_to_one_node(my_node,msg->node_id,
                        sent_msg,LEADER_ELECTION_MSG_SIZE,"Prepare Ack Msg");
                free(sent_msg);
            }
        }
    }else if(msg->type==LELE_ACCEPT){
        if(msg->pnum>=mod->acceptor.highest_seen_pnum){
            int ret = 1;
            mod->acceptor.highest_seen_pnum = msg->pnum;
            mod->acceptor.accepted_pnum = msg->pnum;
            mod->acceptor.content = msg->content;
            do{
                ret = acceptor_update_record(my_node);
                if(ret){
                    SYS_LOG(my_node,"Database Operation Failed in %s,Try Agagin.\n",
                            __PRETTY_FUNCTION__);
                }
            }while(ret);
            sent_msg = build_lele_msg(my_node->node_id,
                    mod,LELE_ACCEPT_ACK,NULL);
            sent_msg->vc_msg.tail_data.last_req = 
                my_node->highest_to_commit.req_id;
            // update it self learner record
            if(NULL!=sent_msg){
                leader_election_learner_do(my_node,mod,&sent_msg->vc_msg);
                send_to_other_nodes(my_node,
                        sent_msg,LEADER_ELECTION_MSG_SIZE,"Accept Ack Msg");
                free(sent_msg);
            }
        }
    }else{
        SYS_LOG(my_node,"Unknown Type of Msg Got in %s.\n",__PRETTY_FUNCTION__);
    }
    DEBUG_LEAVE
    return;
};

static int learner_check_accept(node* my_node,lele_mod* mod,lele_msg* ret_msg){
    DEBUG_ENTER
    int g_half_size = my_node->group_size/2;
    ret_msg->pnum = -1;
    ret_msg->content = -1;
    int inc = 0;
    pnum_t origin = mod->next_pnum-my_node->group_size;
    uint32_t i;
    forlessp(i,0,my_node->group_size){
        if(mod->proposer_arr[i].p_pnum == origin){
            if(mod->proposer_arr[i].a_pnum != -1){
                inc++;
            }
            if(mod->proposer_arr[i].a_pnum > ret_msg->pnum){
                ret_msg->content = mod->proposer_arr[i].content;
                ret_msg->pnum = mod->proposer_arr[i].a_pnum;
            }
        }
    }
    ret_msg->pnum = origin;
    return (inc>=g_half_size);
    DEBUG_LEAVE
    return 0;
};

static void leader_election_send_boundary(node* my_node,lele_mod* mod,lele_msg* msg){
    return;
}

static void leader_election_cal_boundary(node* my_node,lele_mod* mod,lele_msg* msg){
    return;
}

static void leader_election_close(node* my_node,lele_mod* mod,lele_msg* msg){
    return;
}

static void leader_election_learner_do(node* my_node,lele_mod* mod,lele_msg* msg){
    DEBUG_ENTER
    assert(NULL!=msg&&"We Get Here Because We've Got Some Message.\n");
    leader_election_msg* sent_msg = NULL;
    // double-check
    if(msg->type==LELE_ACCEPT_ACK){
        if((msg->pnum)>(mod->learner_arr[msg->node_id].pnum)){
            mod->learner_arr[msg->node_id].pnum = msg->pnum;
            mod->learner_arr[msg->node_id].content = msg->content;
            mod->learner_arr[msg->node_id].last_req = msg->tail_data.last_req;
            lele_msg temp;
            if(learner_check_accept(my_node,mod,&temp)){
                sent_msg = build_lele_msg(my_node->node_id,mod,LELE_ANNOUNCE,&temp);
                if(NULL!=sent_msg){
                    send_to_other_nodes(my_node,sent_msg,
                            LEADER_ELECTION_MSG_SIZE,"Leader Election Announce Msg");
                }
                // i am the new leader
                if(my_node->node_id==sent_msg->vc_msg.content){

                }else{

                }
            }
        }
    }else{
        SYS_LOG(my_node,"Unknown Type of Msg Got in %s.\n",__PRETTY_FUNCTION__);
    }
    DEBUG_LEAVE
    return;
}
static void leader_election_finalize(node* my_node,lele_mod* mod,lele_msg* msg){
    DEBUG_ENTER
    my_node->election_mod->final_state = 1;
    assert(NULL!=msg&&"We Get Here Because We've Got Some Message.\n");
    DEBUG_LEAVE
    return;
}

static void handle_leader_election_msg(node* my_node,leader_election_msg* buf_msg){
    DEBUG_ENTER
    SYS_LOG(my_node,"Node %d Received Leader Election Msg.\n",
            my_node->node_id);
    lele_msg* msg = &buf_msg->vc_msg;
    assert(msg!=NULL&&"We get here because we've got the message.");
    if(msg->next_view!=my_node->cur_view.view_id+1){
        // sender is lagged
        if(msg->next_view<=my_node->cur_view.view_id){
            lele_msg temp;
            temp.next_view = my_node->cur_view.view_id;
            temp.content = my_node->cur_view.leader_id;
            leader_election_msg* lag_msg = build_lele_msg(my_node->node_id,
                my_node->election_mod,LELE_LAGGED,&temp);
            if(NULL!=lag_msg){
                send_to_one_node(my_node,msg->node_id,
                        lag_msg,LEADER_ELECTION_MSG_SIZE,
                        "Lag Node Msg");
                free(lag_msg);
            }
        }// else we are behind, we just set the node to be inactive and wait for the ping msg from the leader
        else{
            if(msg->type==LELE_LAGGED){
                view new_view;
                new_view.leader_id = msg->content;
                new_view.view_id = msg->next_view;
                update_view(my_node,&new_view);
            }else{
                my_node->state = NODE_WAITFORSYNC;
            }
        }
    }else{
        if(NODE_INLELE==my_node->state){
            switch(msg->type){
                case LELE_PREPARE:
                    leader_election_acceptor_do(my_node,my_node->election_mod,msg);
                    break;
                case LELE_PREPARE_ACK:
                    leader_election_proposer_do(my_node,my_node->election_mod,msg);
                    break;
                case LELE_ACCEPT:
                    leader_election_acceptor_do(my_node,my_node->election_mod,msg);
                    break;
                case LELE_ACCEPT_ACK:
                    leader_election_learner_do(my_node,my_node->election_mod,msg);
                    break;
                case LELE_ANNOUNCE:
                    leader_election_finalize(my_node,my_node->election_mod,msg);
                    break;
                case LELE_ANNOUNCE_ACK:
                    leader_election_cal_boundary(my_node,my_node->election_mod,msg);
                    break;
                case LELE_FIN:
                    leader_election_close(my_node,my_node->election_mod,msg);
                    break;
                // temporarily not useful
                case LELE_HIGHER_NODE:
                    leader_election_proposer_do(my_node,my_node->election_mod,msg);
                    break;
                default:
                SYS_LOG(my_node,"Unknown Message Got.\n");
                break;
            }
        }
        //else{
        //ignore this, because we can still communicate to the leader.
        //}
    }
    CHECK_EXIT
    DEBUG_LEAVE return;
}

static void handle_msg(node* my_node,struct bufferevent* bev,size_t data_size){
    //debug_log("there is enough data to read,actual data handler is called\n");
    void* msg_buf = (char*)malloc(SYS_MSG_HEADER_SIZE+data_size);
    if(NULL==msg_buf){
        goto handle_msg_exit;
    }
    struct evbuffer* evb = bufferevent_get_input(bev);
    evbuffer_remove(evb,msg_buf,SYS_MSG_HEADER_SIZE+data_size);
    sys_msg_header* msg_header = msg_buf;
    switch(msg_header->type){
        case PING_ACK:
                handle_ping_ack(my_node,(ping_ack_msg*)msg_buf);
            break;
        case PING_REQ:
            if(!(my_node->state==NODE_INLELE&&(!my_node->election_mod->final_state))){
                handle_ping_req(my_node,(ping_req_msg*)msg_buf);
            }
            break;
        case CONSENSUS_MSG:
            if(my_node->state==NODE_ACTIVE){
                handle_consensus_msg(my_node,(consensus_msg*)msg_buf);
            }
            break;
        case REQUEST_SUBMIT:
            if(my_node->state==NODE_ACTIVE){
                handle_request_submit(my_node,(req_sub_msg*)msg_buf,bev);
            }
            break;
        case LEADER_ELECTION_MSG:
            handle_leader_election_msg(my_node,(leader_election_msg*)msg_buf);
            break;
        default:
            SYS_LOG(my_node,"Unknown Msg Type %d\n",
                    msg_header->type);
            goto handle_msg_exit;
    }

handle_msg_exit:
    if(NULL!=msg_buf){free(msg_buf);}
    CHECK_EXIT
    return;
}

//general data handler by the user, test if there is enough data to read
static void replica_on_read(struct bufferevent* bev,void* arg){
    node* my_node = arg;
    sys_msg_header* buf = NULL;;
    struct evbuffer* input = bufferevent_get_input(bev);
    size_t len = 0;
    len = evbuffer_get_length(input);
    SYS_LOG(my_node,"Enter Consensus Communication Module.\n");
    int counter = 0;
    SYS_LOG(my_node,"There Is %u Bytes Data In The Buffer In Total.\n",
            (unsigned)len);
    while(len>=SYS_MSG_HEADER_SIZE){
        buf = (sys_msg_header*)malloc(SYS_MSG_HEADER_SIZE);
        if(NULL==buf){return;}
        evbuffer_copyout(input,buf,SYS_MSG_HEADER_SIZE);
        int data_size = buf->data_size;
        if(len>=(SYS_MSG_HEADER_SIZE+data_size)){
           my_node->msg_cb(my_node,bev,data_size); 
           counter++;
        }else{
            break;
        }
        free(buf);
        buf=NULL;
        len = evbuffer_get_length(input);
    }
    if(my_node->stat_log){
        //STAT_LOG(my_node,"This Function Call Process %u Requests In Total.\n",counter);
    }
    if(NULL!=buf){free(buf);}
    CHECK_EXIT
    return;
}


int initialize_node(node* my_node,const char* log_path,int deliver_mode,void (*user_cb)(size_t data_size,void* data,void* arg),void* db_ptr,void* arg){
    
    int flag = 1;
    gettimeofday(&my_node->last_ping_msg,NULL);
    if(my_node->cur_view.leader_id==my_node->node_id){
        if(initialize_leader_ping(my_node)){
            goto initialize_node_exit;
        }
        if(initialize_leader_make_progress(my_node)){
            goto initialize_node_exit;
        }
    }
    else{
        if(initialize_expect_ping(my_node)){
            goto initialize_node_exit;
        }
    }

    int build_log_ret = 0;
    if(log_path==NULL){
        log_path = ".";
    }else{
        if((build_log_ret=mkdir(log_path,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))!=0){
            if(errno!=EEXIST){
                err_log("CONSENSUS MODULE : Log Directory Creation Failed,No Log Will Be Recorded.\n");
            }else{
                build_log_ret = 0;
            }
        }
    }
    if(!build_log_ret){
        //if(my_node->sys_log || my_node->stat_log){
            char* sys_log_path = (char*)malloc(sizeof(char)*strlen(log_path)+50);
            memset(sys_log_path,0,sizeof(char)*strlen(log_path)+50);
            if(NULL!=sys_log_path){
                sprintf(sys_log_path,"%s/node-%u-consensus-sys.log",log_path,my_node->node_id);
                my_node->sys_log_file = fopen(sys_log_path,"w");
                free(sys_log_path);
            }
            if(NULL==my_node->sys_log_file && (my_node->sys_log || my_node->stat_log)){
                err_log("CONSENSUS MODULE : System Log File Cannot Be Created.\n");
            }
        //}
    }
//    my_node->signal_handler = evsignal_new(my_node->base,
//            SIGQUIT,node_singal_handler,my_node);
//    evsignal_add(my_node->signal_handler,NULL);
    
    my_node->state = NODE_ACTIVE;
    my_node->msg_cb = handle_msg;
    connect_peers(my_node);
    my_node->consensus_comp = NULL;

    my_node->consensus_comp = init_consensus_comp(my_node,
            my_node->node_id,my_node->sys_log_file,my_node->sys_log,
            my_node->stat_log,my_node->db_name,deliver_mode,db_ptr,my_node->group_size,
            &my_node->cur_view,&my_node->highest_to_commit,&my_node->highest_committed,&my_node->highest_seen,user_cb,send_for_consensus_comp,arg);
    if(NULL==my_node->consensus_comp){
        goto initialize_node_exit;
    }
    flag = 0;
initialize_node_exit:
    return flag;
}

node* system_initialize(node_id_t node_id,const char* start_mode,const char* config_path,const char* log_path,int deliver_mode,void(*user_cb)(int data_size,void* data,void* arg),void* db_ptr,void* arg){

    node* my_node = (node*)malloc(sizeof(node));
    memset(my_node,0,sizeof(node));
    if(NULL==my_node){
        goto exit_error;
    }
    // set up base
	struct event_base* base = event_base_new();
    if(NULL==base){
        goto exit_error;
    }

    my_node->base = base;
    my_node->node_id = node_id;
    my_node->db_ptr = db_ptr;
    //seed, currently the node is the leader
    if(*start_mode=='s'){
        my_node->cur_view.view_id = 1;
        my_node->cur_view.leader_id = my_node->node_id;
        my_node->ev_leader_ping = NULL;
    }else{
        my_node->cur_view.view_id = 0;
        my_node->cur_view.leader_id = 9999;
        my_node->ev_leader_ping = NULL;
    }
    
    my_node->config.make_progress_timeval.tv_sec = 1;
    my_node->config.make_progress_timeval.tv_usec = 100;
    my_node->config.ping_timeval.tv_sec = 2;
    my_node->config.ping_timeval.tv_usec = 0;
    my_node->config.expect_ping_timeval.tv_sec = 8;
    my_node->config.expect_ping_timeval.tv_usec = 0;
    my_node->config.reconnect_timeval.tv_sec = 2;
    my_node->config.reconnect_timeval.tv_usec = 0;

    if(consensus_read_config(my_node,config_path)){
        err_log("CONSENSUS MODULE : Configuration File Reading Failed.\n");
        goto exit_error;
    }


    if(initialize_node(my_node,log_path,deliver_mode,user_cb,db_ptr,arg)){
        err_log("CONSENSUS MODULE : Network Layer Initialization Failed.\n");
        goto exit_error;
    }

    my_node->listener =
        evconnlistener_new_bind(base,replica_on_accept,
                (void*)my_node,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,-1,
                (struct sockaddr*)&my_node->my_address,sizeof(my_node->my_address));

    if(!my_node->listener){
        err_log("CONSENSUS MODULE : Cannot Set Up The Listener.\n");
        goto exit_error;
    }
	return my_node;

exit_error:
    if(NULL!=my_node){
        free_node(my_node);
    }
    return NULL;
}

void system_run(struct node_t* my_node){
    SYS_LOG(my_node,"Node %u Starts Running\n",
            my_node->node_id);
    sigset_t node_sig_set;
    sigfillset(&node_sig_set);
    sigdelset(&node_sig_set,SIGUSR1);
    int ret = pthread_sigmask(SIG_BLOCK,&node_sig_set,NULL);
    signal(SIGUSR1,node_sys_sig_handler);
    event_base_dispatch(my_node->base);
}

void system_exit(struct node_t* my_node){
    free_node(my_node);
}
