#include "../include/util/common-header.h"
#include "../include/replica-sys/node.h"
#include "../include/config-comp/config-comp.h"
#include "../include/replica-sys/replica.h"


int max_waiting_connections = MAX_ACCEPT_CONNECTIONS; 
static unsigned current_connection = 3;
struct timeval reconnect_timeval = {2,0};
struct timeval ping_timeval = {2,0};
struct timeval expect_ping_timeval = {8,0};
struct timeval make_progress_timeval = {2,0};
int heart_beat_threshold = 4;

static void usage(){
    paxos_log("Usage : -n NODE_ID\n");
    paxos_log("        -m [sr] Start Mode seed|recovery\n");
    paxos_log("        -c path path to configuration file\n");
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
static void update_view(node*,view*);
static void become_leader(node*);
static void giveup_leader(node*);
static void replica_sync(node*);

//free related function
static void free_peers(node*);
static int free_node(node*);

//helper function
static int isLeader(node*);

//implementation level

static void node_singal_handler(evutil_socket_t fid,short what,void* arg){
    ENTER_FUNC
    node* my_node = arg;
    if(what&EV_SIGNAL){
        debug_log("Node %d Received Kill Singal.Now Quit.\n",my_node->node_id);
    }
    event_base_loopexit(my_node->base,NULL);
    LEAVE_FUNC
}

//static void node_sys_sig_handler(int sig){
//    ENTER_FUNC
//    switch(sig){
//        case SIGTERM:
//        case SIGHUP:
//        case SIGKILL:
//        case SIGINT:
//            paxos_log("catch one singal\n");
//            break;
//    }
//    LEAVE_FUNC
//}

static void peer_node_on_read(struct bufferevent* bev,void* arg){return;};

static void peer_node_on_event(struct bufferevent* bev,short ev,void* arg){
    peer* peer_node = (peer*)arg;
    if(ev&BEV_EVENT_CONNECTED){
        debug_log("Connected to Node %d\n",peer_node->peer_id);
        peer_node->active = 1;
    }else if((ev & BEV_EVENT_EOF )||(ev&BEV_EVENT_ERROR)){
        if(peer_node->active){
            peer_node->active = 0;
            debug_log("Lost Connection With Node %d \n",peer_node->peer_id);
        }
        peer_node->my_buff_event = NULL;
        int err = EVUTIL_SOCKET_ERROR();
		debug_log("%s (%d)\n",evutil_socket_error_to_string(err),peer_node->peer_id);
        bufferevent_free(bev);
        event_add(peer_node->reconnect,&reconnect_timeval);
    }
};


static void connect_peer(peer* peer_node){
    peer_node->my_buff_event = bufferevent_socket_new(peer_node->base,-1,BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(peer_node->my_buff_event,peer_node_on_read,NULL,peer_node_on_event,peer_node);
    bufferevent_enable(peer_node->my_buff_event,EV_READ|EV_WRITE|EV_PERSIST);
    bufferevent_socket_connect(peer_node->my_buff_event,(struct sockaddr*)peer_node->peer_address,peer_node->sock_len);
};

static void peer_node_on_timeout(int fd,short what,void* arg){
    connect_peer((peer*)arg);
};


static void connect_peers(node* my_node){
    for(uint32_t i=0;i<my_node->group_size;i++){
        if(i!=my_node->node_id){
            peer* peer_node = my_node->peer_pool+i;
            peer_node->reconnect = evtimer_new(peer_node->base,peer_node_on_timeout,peer_node);
            connect_peer(peer_node);
        }
    }
}


static void lost_connection_with_leader(node* my_node){
    debug_log("Node %u Lost Connection With The Leader\n",
            my_node->node_id);
    debug_log("Node %u Will Start A Leader Election\n",
            my_node->node_id);
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
        timeval_add(last,&expect_ping_timeval,&temp);
        if(timeval_comp(&temp,&cur)>=0){
            event_add(my_node->ev_leader_ping,&expect_ping_timeval);
        }else{
            debug_log(
                    "Node %d haven't heard from the leader\n",
                    my_node->node_id);
            lost_connection_with_leader(my_node);
        }
    }
}


static void leader_ping_period(int fd,short what,void* arg){
    debug_log("Leader Tries To Ping Other Nodes\n");
    node* my_node = arg; 
    // at first check whether I am the leader
    if(my_node->cur_view.leader_id!=my_node->node_id){
        if(my_node->ev_leader_ping!=NULL){
            event_free(my_node->ev_leader_ping);
            initialize_expect_ping(my_node);
        }
        return;
    }else{
        void* ping_req = build_ping_req(my_node->node_id,&my_node->cur_view);
        if(NULL==ping_req){
            goto add_ping_event;
        }
        for(uint32_t i=0;i<my_node->group_size;i++){
            if(i!=my_node->node_id && my_node->peer_pool[i].active){
                struct bufferevent* buff = my_node->peer_pool[i].my_buff_event;
                bufferevent_write(buff,ping_req,PING_REQ_SIZE);
                debug_log(
                        "Send Ping Msg To Node %u\n",i);
            }
        }
        if(NULL!=ping_req){
            free(ping_req);
        }
    add_ping_event:
        if(NULL!=my_node->ev_leader_ping){
            event_add(my_node->ev_leader_ping,&ping_timeval);
        }
    }
};

static int initialize_leader_ping(node* my_node){
    if(NULL==my_node->ev_leader_ping){
        my_node->ev_leader_ping = evtimer_new(my_node->base,leader_ping_period,(void*)my_node);
        if(my_node->ev_leader_ping==NULL){
            return 1;
        }
    }
    event_add(my_node->ev_leader_ping,&ping_timeval);
    return 0;
}

static int initialize_expect_ping(node* my_node){
    if(NULL==my_node->ev_leader_ping){
        my_node->ev_leader_ping = evtimer_new(my_node->base,expected_leader_ping_period,(void*)my_node);
        if(my_node->ev_leader_ping==NULL){
            return 1;
        }
    }
    event_add(my_node->ev_leader_ping,&expect_ping_timeval);
    return 0;
}

static void make_progress_on(int fd,short what,void* arg){
    debug_log("Leader Tries To Ping Other Nodes\n");
    node* my_node = arg; 
    // at first check whether I am the leader
    if(my_node->cur_view.leader_id!=my_node->node_id){
        if(my_node->ev_make_progress!=NULL){
            event_free(my_node->ev_make_progress);
            initialize_leader_make_progress(my_node);
        }
        return;
    }
    if(NULL!=my_node->consensus_comp){
        consensus_make_progress(my_node->consensus_comp);
    }
    if(NULL!=my_node->ev_make_progress){
        event_add(my_node->ev_make_progress,&make_progress_timeval);
    }
    return;
}

static int initialize_leader_make_progress(node* my_node){
    if(NULL==my_node->ev_make_progress){
        my_node->ev_make_progress = evtimer_new(my_node->base,make_progress_on,(void*)my_node);
        if(my_node->ev_make_progress==NULL){
            return 1;
        }
    }
    event_add(my_node->ev_make_progress,&make_progress_timeval);
    return 0;
}



static void update_view(node* my_node,view* new_view){
    debug_log("node %d entered update view\n",
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
    debug_log("node %d 's current view changed to %u \n",
            my_node->node_id,
            my_node->cur_view.view_id);
    return;
}

static void become_leader(node* my_node){
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
}

static void free_peers(node* my_node){
    for(uint32_t i=0;i<my_node->group_size;i++){
        free_peer(my_node->peer_pool+i);
    }
}

static int isLeader(node* my_node){
    return my_node->cur_view.leader_id==my_node->node_id;
}

static int free_node(node* my_node){
    ENTER_FUNC
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
    LEAVE_FUNC
    return 0;
}


static void replica_on_error_cb(struct bufferevent* bev,short ev,void *arg){
    if(ev&BEV_EVENT_EOF){
        // the connection has been closed;
        // do the cleaning
    }
    return;
}

static void replica_on_accept(struct evconnlistener* listener,evutil_socket_t fd,struct sockaddr *address,int socklen,void *arg){
    node* my_node = arg;
    debug_log( "In consensus,Connection is established.\n");
    if(current_connection>=MAX_ACCEPT_CONNECTIONS){
        debug_log("it has exceeded the predefined maximal concurrent connections\n");
        close(fd);
    }else{
        struct bufferevent* new_buff_event = bufferevent_socket_new(my_node->base,fd,BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(new_buff_event,replica_on_read,NULL,replica_on_error_cb,(void*)my_node);
        bufferevent_enable(new_buff_event,EV_READ|EV_PERSIST|EV_WRITE);
        current_connection++;
    }
};

// consensus part
static void send_for_consensus_comp(node* my_node,size_t data_size,void* data,int target){
    ENTER_FUNC
    consensus_msg* msg = build_consensus_msg(data_size,data);
    if(NULL==msg){
        goto send_for_consensus_comp_exit;
    }
    // means send to every node except me
    if(target<0){
        for(uint32_t i=0;i<my_node->group_size;i++){
            if(i!=my_node->node_id && my_node->peer_pool[i].active){
                struct bufferevent* buff = my_node->peer_pool[i].my_buff_event;
                bufferevent_write(buff,msg,CONSENSUS_MSG_SIZE(msg));
                debug_log(
                        "Send Consensus Msg To Node %u\n",i);
            }
        }
    }else{
        if(target!=(int)my_node->node_id&&my_node->peer_pool[target].active){
            struct bufferevent* buff = my_node->peer_pool[target].my_buff_event;
            bufferevent_write(buff,msg,CONSENSUS_MSG_SIZE(msg));
            debug_log(
                    "Send Consensus Msg To Node %u\n",target);
        }
    }
send_for_consensus_comp_exit:
    if(msg!=NULL){
        free(msg);
    }
    LEAVE_FUNC
    return;
}

static void handle_ping_ack(node* my_node,ping_ack_msg* msg){
    if(my_node->cur_view.view_id < msg->view.view_id){
        update_view(my_node,&msg->view);
    }else{
        debug_log("ignore ping ack from node %u \n",msg->node_id);
    }
}

static void handle_ping_req(node* my_node,ping_req_msg* msg){
    debug_log("Received Ping Req Msg in node %d\n from node %d\n",
            my_node->node_id,msg->node_id);
    if(my_node->cur_view.view_id < msg->view.view_id){
        update_view(my_node,&msg->view);
    }else if(my_node->cur_view.view_id > msg->view.view_id){
        if(my_node->peer_pool[msg->node_id].active){
            void* ping_ack = build_ping_ack(my_node->node_id,&my_node->cur_view);
            if(NULL!=ping_ack){
                struct bufferevent* buff = my_node->peer_pool[msg->node_id].my_buff_event;
                bufferevent_write(buff,ping_ack,PING_REQ_SIZE);
                debug_log(
                    "Send Ping Ack To Lagged Node %u\n",msg->node_id);
                free(ping_ack);
            }
        } 
        return;
    }
    if(my_node->cur_view.view_id == msg->view.view_id){
        if(!isLeader(my_node)){
            if(timeval_comp(&my_node->last_ping_msg,&msg->timestamp)<0){
                memcpy(&my_node->last_ping_msg,&msg->timestamp,
                        sizeof(struct timeval));
            }
            if(NULL!=my_node->ev_leader_ping){
                evtimer_del(my_node->ev_leader_ping);
                evtimer_add(my_node->ev_leader_ping,&expect_ping_timeval);
            }
        }else{
            // leader should not receive the ping req, otherwise the sender is
            // lagged behind,otherwise the leader is outdated which will be
            // treated as a smaller cur view than what in the msg but when they have 
            // the same view id, which can be ignored
            debug_log("Received Ping Req From %u in view %u\n",
                    msg->node_id,msg->view.view_id);
        }
    }
    return;
}

static void handle_consensus_msg(node* my_node,consensus_msg* msg){
    debug_log("node %d received consensus message\n",
            my_node->node_id);
    if(NULL!=my_node->consensus_comp){
        consensus_handle_msg(my_node->consensus_comp,msg->header.data_size,(void*)msg+SYS_MSG_HEADER_SIZE);
    }
    return;
}

static void handle_request_submit(node* my_node,
        req_sub_msg* msg,struct bufferevent* evb){
    debug_log("node %d received consensus submit request\n",
            my_node->node_id);
    debug_log("the data size is %lu \n",
            msg->header.data_size);
    if(NULL!=my_node->consensus_comp){
        view_stamp return_vs;
        consensus_submit_request(
                my_node->consensus_comp,msg->header.data_size,
                (void*)msg+SYS_MSG_HEADER_SIZE,&return_vs);
        // build_return_message;
        //bufferevent_write(evb,)
    }
    return;
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
            handle_ping_req(my_node,(ping_req_msg*)msg_buf);
            break;
        case CONSENSUS_MSG:
            handle_consensus_msg(my_node,(consensus_msg*)msg_buf);
            break;
        case REQUEST_SUBMIT:
            handle_request_submit(my_node,(req_sub_msg*)msg_buf,bev);
            break;
        default:
            debug_log("unknown msg type %d\n",
                    msg_header->type);
            goto handle_msg_exit;
    }

handle_msg_exit:
    if(NULL!=msg_buf){free(msg_buf);}
    return;
}

//general data handler by the user, test if there is enough data to read
static void replica_on_read(struct bufferevent* bev,void* arg){
    node* my_node = arg;
    sys_msg_header* buf = NULL;;
    struct evbuffer* input = bufferevent_get_input(bev);
    size_t len = 0;
    len = evbuffer_get_length(input);
    paxos_log("Enter Consensus Communication Module.\n");
    int counter = 0;
    debug_log("there is %u bytes data in the buffer in total\n",
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
    paxos_log("This Function Call Process %u Requests In Total.\n",counter);
    if(NULL!=buf){free(buf);}
    return;
}


int initialize_node(node* my_node,void (*user_cb)(size_t data_size,void* data,void* arg),void* arg){
    ENTER_FUNC
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
     
    my_node->signal_handler = evsignal_new(my_node->base,
            SIGQUIT,node_singal_handler,my_node);
    evsignal_add(my_node->signal_handler,NULL);
    my_node->state = NODE_ACTIVE;
    my_node->msg_cb = handle_msg;
    connect_peers(my_node);
    my_node->consensus_comp = NULL;

    my_node->consensus_comp = init_consensus_comp(my_node,
            my_node->node_id,my_node->db_name,my_node->group_size,
            &my_node->cur_view,user_cb,send_for_consensus_comp,arg);
    if(NULL==my_node->consensus_comp){
        goto initialize_node_exit;
    }
    flag = 0;
initialize_node_exit:
        return flag;
}

node* system_initialize(int node_id,const char* start_mode,const char* config_path,void(*user_cb)(int data_size,void* data,void* arg),void* arg){
    ENTER_FUNC
    
//    signal(SIGINT,node_sys_sig_handler);
//    signal(SIGHUP,node_sys_sig_handler);
//    signal(SIGTERM,node_sys_sig_handler);
//    signal(SIGQUIT,node_sys_sig_handler);

    DEBUG_POINT(1);
    node* my_node = (node*)malloc(sizeof(node));
    memset(my_node,0,sizeof(node));
    if(NULL==my_node){
        goto exit_error;
    }

    DEBUG_POINT(2);

    // set up base
	struct event_base* base = event_base_new();


    if(NULL==base){
        goto exit_error;
    }

	//int s_fd = socket(AF_INET,SOCK_STREAM,0);

    my_node->base = base;
    my_node->node_id = node_id;

    DEBUG_POINT(3)
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

    DEBUG_POINT(4)
    if(consensus_read_config(my_node,config_path)){
        goto exit_error;
    }
    
#if DEBUG
    char ipv4_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&my_node->my_address.sin_addr,ipv4_address,INET_ADDRSTRLEN);
    debug_log("current node's ip address is %s:%d\n",ipv4_address,ntohs(my_node->my_address.sin_port));
    debug_log("current node's db name is %s\n",my_node->db_name);
#endif

    DEBUG_POINT(5)
    if(initialize_node(my_node,user_cb,arg)){
        debug_log("cannot initialize node\n");
        goto exit_error;
    }

    my_node->listener =
        evconnlistener_new_bind(base,replica_on_accept,
                (void*)my_node,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,-1,
                (struct sockaddr*)&my_node->my_address,sizeof(my_node->my_address));

    DEBUG_POINT(6);
    if(!my_node->listener){
        debug_log("cannot set up the listener\n");
        goto exit_error;
    }

    LEAVE_FUNC

	return my_node;

exit_error:
    if(NULL!=my_node){
        free_node(my_node);
    }
    return NULL;
}

void system_run(struct node_t* replica){
    ENTER_FUNC
    debug_log("Node %u Starts Running\n",
            replica->node_id);
    event_base_dispatch(replica->base);
    LEAVE_FUNC
}


void system_exit(struct node_t* replica){
    ENTER_FUNC
    event_base_loopexit(replica->base,NULL);
    DEBUG_POINT(0)
    DEBUG_POINT(1)
    free_node(replica);
    LEAVE_FUNC
}
