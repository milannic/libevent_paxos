#include "common-header.h"
#include "node.h"
#include "config-comp.h"
#include "replica.h"

int max_waiting_connections = MAX_ACCEPT_CONNECTIONS; 
static unsigned current_connection = 3;
struct timeval reconnect_timeval = {2,0};
struct timeval ping_timeval = {2,0};
struct timeval expect_ping_timeval = {10,0};
int heart_beat_threshold = 4;

static void usage(){
    paxos_log("Usage : -n NODE_ID\n");
    paxos_log("        -m [sr] Start Mode seed|recovery\n");
    paxos_log("        -c path path to configuration file\n");
}



//msg handler
static void replica_on_read(struct bufferevent* bev,void* arg);
static void replica_on_error_cb(struct bufferevent* bev,short ev,void* arg);

//concrete message
static void handle_ping_ack(node* my_node,ping_ack_msg* msg);
static void handle_ping_req(node* my_node,ping_req_msg* msg);


//listener
static void replica_on_accept(struct evconnlistener* listener,evutil_socket_t
        fd,struct sockaddr *address,int socklen,void *arg);

//peer connection
static void peer_node_on_event(struct bufferevent* bev,short ev,void* arg);
static void peer_node_on_read(struct bufferevent* bev,void* arg);
static void connect_peer(peer* peer_node);
static void peer_node_on_timeout(int fd,short what,void* arg);
static void connect_peers(node* my_node);


//ping part
static void leader_ping_period(int fd,short what,void* arg);
static void expected_leader_ping_period(int fd,short wat,void* arg);
static int initialize_leader_ping(node* my_node);
static int initialize_expect_ping(node* my_node);

//view change
static void update_view(node* my_node,view* new_view);
static void become_leader(node* my_node);
static void giveup_leader(node* my_node);
static void replica_sync(node* my_node);

//free related function
static void free_peers(node* my_node);
static int free_node(node* my_node);

//helper function
static int isLeader(node* node);


static void peer_node_on_read(struct bufferevent* bev,void* arg){return;};
static void peer_node_on_event(struct bufferevent* bev,short ev,void* arg){
    peer* peer_node = (peer*)arg;
    if(ev&BEV_EVENT_CONNECTED){
        paxos_log("Connected to Node %d\n",peer_node->peer_id);
        peer_node->active = 1;
    }else if((ev & BEV_EVENT_EOF )||(ev&BEV_EVENT_ERROR)){
        if(peer_node->active){
            peer_node->active = 0;
            paxos_log("Lost Connection With Node %d \n",peer_node->peer_id);
        }
        int err = EVUTIL_SOCKET_ERROR();
		paxos_log("%s (%d)\n",evutil_socket_error_to_string(err),peer_node->peer_id);
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
    paxos_log("Node %u Lost Connection With The Leader\n",
            my_node->node_id);
    paxos_log("Node %u Will Start A Leader Election\n",
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
    paxos_log("Leader Tries To Ping Other Nodes\n");
    node* my_node = arg; 
    // at first check whether I am the leader
    if(my_node->cur_view.leader_id!=my_node->node_id){
        if(my_node->ev_leader_ping!=NULL){
            event_free(my_node->ev_leader_ping);
            initialize_expect_ping(my_node);
        }
        return;
    }else{
        sys_msg* ping_req = build_ping_req(my_node->node_id,&my_node->cur_view);
        if(NULL==ping_req){
            goto add_ping_event;
        }
        for(uint32_t i=0;i<my_node->group_size;i++){
            if(i!=my_node->node_id && my_node->peer_pool[i].active){
                struct bufferevent* buff = my_node->peer_pool[i].my_buff_event;
                bufferevent_write(buff,ping_req,SYS_MSG_SIZE((ping_req->data_size)));
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


static void update_view(node* my_node,view* new_view){
    debug_log("entered update view\n");
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
    debug_log("leave update view\n");
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
    //to-do
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
    paxos_log( "a new connection is established and the socket is %d\n",fd);
    if(current_connection>=MAX_ACCEPT_CONNECTIONS){
        paxos_log("it has exceeded the predefined maximal concurrent connections\n");
        close(fd);
    }else{
        struct bufferevent* new_buff_event = bufferevent_socket_new(my_node->base,fd,BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(new_buff_event,replica_on_read,NULL,replica_on_error_cb,(void*)my_node);
        bufferevent_enable(new_buff_event,EV_READ|EV_PERSIST|EV_WRITE);
        current_connection++;
    }
};


static void handle_ping_ack(node* my_node,ping_ack_msg* msg){
    if(my_node->cur_view.view_id < msg->view.view_id){
        update_view(my_node,&msg->view);
    }else{
        debug_log("ignore ping ack from node %u \n",msg->node_id);
    }
}

static void handle_ping_req(node* my_node,ping_req_msg* msg){
    if(my_node->cur_view.view_id < msg->view.view_id){
        update_view(my_node,&msg->view);
    }else if(my_node->cur_view.view_id > msg->view.view_id){
        if(my_node->peer_pool[msg->node_id].active){
            sys_msg* ping_ack = build_ping_ack(my_node->node_id,&my_node->cur_view);
            if(NULL!=ping_ack){
                struct bufferevent* buff = my_node->peer_pool[msg->node_id].my_buff_event;
                bufferevent_write(buff,ping_ack,SYS_MSG_SIZE((ping_ack->data_size)));
                debug_log(
                    "Send Ping Ack To Lagged Node %u\n",msg->node_id);
                free(ping_ack);
            }
        } 
    }else{
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
}


static void handle_msg(node* my_node,struct evbuffer* evb){
    debug_log("there is enough data to read,actual data handler is called\n");
    char* under_msg_buf=NULL;
    char* sys_msg_buf=(char*)malloc(sizeof(sys_msg));
    if(NULL==sys_msg_buf){return;}
    evbuffer_remove(evb,sys_msg_buf,sizeof(sys_msg));
    under_msg_buf = (char*)malloc(((sys_msg*)sys_msg_buf)->data_size);
    assert((under_msg_buf!=NULL)&&
            "cannot allocate memory space for the new received data,the program will exit");
    evbuffer_remove(evb,under_msg_buf,((sys_msg*)sys_msg_buf)->data_size);
    switch(((sys_msg*)sys_msg_buf)->type){
        case ping_ack:
            handle_ping_ack(my_node,(ping_ack_msg*)under_msg_buf);
            break;
        case ping_req:
            handle_ping_req(my_node,(ping_req_msg*)under_msg_buf);
            break;
        default:
            debug_log("unknown msg type %d\n",
                    ((sys_msg*)sys_msg_buf)->type);
    }
    if(NULL!=sys_msg_buf){free(sys_msg_buf);}
    if(NULL!=under_msg_buf){free(under_msg_buf);}
    return;
}

//general data handler by the user, test if there is enough data to read
static void replica_on_read(struct bufferevent* bev,void* arg){
    node* my_node = arg;
    char* buf = NULL;;
    struct evbuffer* input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    debug_log("there is %u bytes data in the buffer in total\n",
            (unsigned)len);
    if(len>sizeof(sys_msg)){
        buf = (char*)malloc(sizeof(sys_msg));
        if(NULL==buf){return;}
        evbuffer_copyout(input,buf,sizeof(sys_msg));
        int data_len = ((sys_msg*)buf)->data_size;
        if(len>sizeof(sys_msg)+data_len){
           my_node->msg_cb(my_node,input); 
        }
        free(buf);
    }
}


int initialize_node(node* my_node){
    int flag = 0;
    gettimeofday(&my_node->last_ping_msg,NULL);
    if(my_node->cur_view.leader_id==my_node->node_id){
        if(initialize_leader_ping(my_node)){
            flag = 1;
            goto initialize_node_exit;
        }
    }
    else{
        if(initialize_expect_ping(my_node)){
            flag = 1;
            goto initialize_node_exit;
        }
    }
    my_node->state = NODE_ACTIVE;
    my_node->msg_cb = handle_msg;
    connect_peers(my_node);
    goto initialize_node_exit;
initialize_node_exit:
        return flag;
}


node* system_initialize(int argc,char** argv,void(*user_cb)(int data_size,void* data)){
    char* start_mode= NULL;
    char* config_path = NULL;
    int node_id = -1;
    int c;

    node* my_node = (node*)malloc(sizeof(node));
    if(NULL==my_node){
        goto exit_error;
    }

    while((c = getopt (argc,argv,"c:n:m:")) != -1){
        switch(c){
            case 'n':
                node_id = atoi(optarg);
                break;
            case 'c':
                config_path = optarg;
                break;
            case 'm':
                start_mode= optarg;
                if(*start_mode!='s' && *start_mode!='r'){
                    paxos_log("Unknown Start Mode\n");
                    usage();
                    goto exit_error;
                }
                break;
            case '?':
                if(optopt == 'n'){
                    paxos_log("Option -n requires an argument\n");
                    usage();
                    goto exit_error;
                }
                else if(optopt == 'm'){
                    paxos_log("Option -m requires an argument\n");
                    usage();
                    goto exit_error;
                }
                else if(optopt == 'c'){
                    paxos_log("Option -c requires an argument\n");
                    usage();
                    goto exit_error;
                }
                break;
            default:
                paxos_log("Unknown Option %d \n",c);
                goto exit_error;
        }
    }

    if(argc<7 || optind>argc){
        usage();
        goto exit_error;
    }

    // set up base
	struct event_base* base = event_base_new();

    if(NULL==base){
        goto exit_error;
    }

	//int s_fd = socket(AF_INET,SOCK_STREAM,0);

    my_node->base = base;
    my_node->node_id = node_id;

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

    if(read_configuration_file(my_node,config_path)){
        goto exit_error;
    }
    
#if DEBUG
    char ipv4_address[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&my_node->my_address.sin_addr,ipv4_address,INET_ADDRSTRLEN);
    debug_log("current node's ip address is %s:%d\n",ipv4_address,ntohs(my_node->my_address.sin_port));
    debug_log("current node's db name is %s\n",my_node->db_name);
#endif


    if(initialize_node(my_node)){
        paxos_log("cannot initialize node\n");
        goto exit_error;
    }

    my_node->listener =
        evconnlistener_new_bind(base,replica_on_accept,
                (void*)my_node,LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE,-1,
                (struct sockaddr*)&my_node->my_address,sizeof(my_node->my_address));

    if(!my_node->listener){
        paxos_log("cannot set up the listener\n");
        goto exit_error;
    }

	return my_node;

exit_error:
    if(NULL!=my_node){
        free_node(my_node);
    }
    return NULL;
}


void system_run(struct node_t* replica){
    event_base_dispatch(replica->base);
}


void system_exit(struct node_t* replica){
    event_base_loopexit(replica->base,NULL);
    event_base_free(replica->base);
    free_node(replica);
}
